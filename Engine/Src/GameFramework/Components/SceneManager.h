#pragma once

#include "Common/DXHelper.h"

template<typename T>
class SceneComponentSet
{
public:
	std::unordered_set<std::shared_ptr<T>> Components;
};

class SceneManager
{
public:

	static SceneManager& Get()
	{
		static SceneManager inst;
		return inst;
	}

	template<typename T>
	SceneComponentSet<T>* GetSceneComponentSet() const
	{
		return GetEmptySet<T>();
	}

	template<typename T>
	static SceneComponentSet<T>& GetEmptySet()
	{
		static SceneComponentSet<T> emptySet;
		return emptySet;
	}

private:
	SceneManager() = default;
	~SceneManager() = default;

	SceneManager(const SceneManager&) = delete;
	SceneManager& operator=(const SceneManager&) = delete;
	SceneManager(SceneManager&&) = delete;
	SceneManager& operator=(SceneManager&&) = delete;
};