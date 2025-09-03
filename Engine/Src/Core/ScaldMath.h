#pragma once

class ScaldMath
{
public:

	template<typename T>
	static T Min(const T& a, const T& b)
	{
		return a < b ? a : b;
	}

	template<typename T>
	static T Max(const T& a, const T& b)
	{
		return a < b ? b : a;
	}

	template<typename T>
	static T Clamp(const T& value, const T& _min, const T& _max)
	{
		return Min(Max(value, _min), _max);
	}
};