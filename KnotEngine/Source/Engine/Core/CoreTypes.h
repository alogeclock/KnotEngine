#pragma once

#include <DirectXMath.h>
#include <array>
#include <memory>
#include <string>
#include <vector>
#include <list>
#include <forward_list>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <queue>
#include <deque>

#include "Core/Assert.h"

template <typename T, typename Allocator = std::allocator<T>>
using TArray = std::vector<T, Allocator>;

template <typename T, typename Allocator = std::allocator<T>>
using TLinkedList = std::forward_list<T, Allocator>;

template <typename T, typename Allocator = std::allocator<T>>
using TDoubleLinkedList = std::list<T, Allocator>;

template <typename KeyType, 
	typename ValueType, 
	typename Hasher = std::hash<KeyType>, 
	typename KeyEqual = std::equal_to<KeyType>,
	typename Allocator = std::allocator<std::pair<const KeyType, ValueType>>>
using TMap = std::unordered_map<KeyType, ValueType, Hasher, KeyEqual, Allocator>;

template <typename FirstType, typename SecondType>
using TPair = std::pair<FirstType, SecondType>;

template <typename T, typename Container = std::deque<T>>
using TQueue = std::queue<T, Container>;

template <typename T, typename Hasher = std::hash<T>, typename KeyEqual = std::equal_to<T>, typename Allocator = std::allocator<T>>
using TSet = std::unordered_set<T, Hasher, KeyEqual, Allocator>;

template <typename T, std::size_t N>
using TStaticArray = std::array<T, N>;

template <typename T>
using TObjectPtr = T*;

using FString = std::string;
using FWString = std::wstring;

using int8 = __int8;
using int16 = __int16;
using int32 = __int32;
using int64 = __int64;

using uint8 = unsigned __int8;
using uint16 = unsigned __int16;
using uint32 = unsigned __int32;
using uint64 = unsigned __int64;

using Float2 = DirectX::XMFLOAT2;
using Float3 = DirectX::XMFLOAT3;
using Float4 = DirectX::XMFLOAT4;

using Float4X4 = DirectX::XMFLOAT4X4;

using SIZE_T = std::size_t;

using ANSICHAR = char;
using WIDECHAR = wchar_t;
using TCHAR = WIDECHAR;
