#pragma once
#include <iostream>
#include <type_traits>
#include <vector>
#include <cstring>
namespace cyh::cni {
	using nuint = size_t;
	using nint = std::make_signed_t<nuint>;

	// constraint the type T should not be class or struct
	template<typename T>
	concept PRIMITIVE = !std::is_class_v<T>;

	class CNIMemoryHelper {
		using This = CNIMemoryHelper;
	public:
		static void Free(void* ptr) {
			if (ptr != nullptr)
				operator delete(ptr);
		}
		static void* Allocate(nuint byteSize) {
			if (byteSize == 0)
				return nullptr;
			try {
				return operator new(byteSize);
			} catch (...) {
				return nullptr;
			}
		}
		static void* Reallocate(void* ptr, nuint byteSize) {
			This::Free(ptr);
			if (byteSize == 0) {
				return nullptr;
			} else {
				return This::Allocate(byteSize);
			}
		}
		template<class T>
		static void ZeroMemory(T* obj, nuint count) {
			if (obj) {
				memset((void*)obj, 0, sizeof(T) * count);
			}
		}
	};

	template<PRIMITIVE T>
	struct CArray {
		// data pointer
		union {
			void* m_block{};
			T* m_data;
		};
		// count of T hold by array
		nuint m_length{};
		constexpr T* begin() const { return this->m_data; }
		constexpr T* end() const { return this->begin() + this->size(); }
		constexpr size_t size() const { return this->m_length; }
	};

	using UTF8String = CArray<char>;

	class CNIConvert {
		template<class TContainer>
		using ContainerValueType = std::decay_t<decltype(*(std::declval<TContainer>().data()))>;
		template<class TContainer>
		[[nodiscard]] static CArray<ContainerValueType<TContainer>> to_primitive_c_array(const TContainer* p_container, bool add_terminator = false) {
			if (!p_container) { return {}; }
			using value_type = ContainerValueType<TContainer>;
			auto elem_count = p_container->size();
			auto elem_size = sizeof(value_type);
			void* block;
			if (add_terminator) {
				block = CNIMemoryHelper::Allocate(elem_size * (elem_count + 1));
			} else {
				block = CNIMemoryHelper::Allocate(elem_size * elem_count);
			}
			if (!block) { return {}; }
			memcpy(block, (void*)p_container->data(), elem_size * elem_count);
			CArray<value_type> result{};
			result.m_block = block;
			result.m_length = elem_count;
			if (add_terminator) {
				memset(result.m_data + elem_count, 0, elem_size);
			}
			return result;
		}
		template<template<class ...> class StdContainer, class T, class... Args>
		static StdContainer<T, Args...> to_std_container(const CArray<T>* c_array, StdContainer<T, Args...>* tp = nullptr) {
			if (!c_array) { return {}; }
			if (!c_array->m_block || !c_array->m_length) { return{}; }
			StdContainer<T, Args...> result{};
			result.resize(c_array->m_length);
			auto data = result.data();
			memcpy((void*)data, c_array->m_block, c_array->m_length * sizeof(T));
			return result;
		}
	public:
		// convert std container to unmanaged array
		template<template<class...> class StdContainer, class T, class...Args>
		[[nodiscard]] static CArray<T> ToCArray(const StdContainer<T, Args...>& cpp_vector) requires(!std::is_class_v<T>) {
			return to_primitive_c_array(&cpp_vector);
		}
		// convert cpp managed string to unmanaged cstring
		[[nodiscard]] static UTF8String ToCNIString(const std::string& str) {
			return to_primitive_c_array(&str, true);
		}
		// convert unmanaged cstring to cpp managed string
		static std::string ToCppString(const UTF8String& u8str) {
			return to_std_container(&u8str, (std::string*)0);
		}
	};

};

