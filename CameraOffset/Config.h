#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <INIReader.h>

#include "resource.h"

class UConfig
{
public:
	static HMODULE ResourceModule;

	static std::string GetDefaultConfig()
	{
		std::string configtxt;
		HRSRC res = FindResource(ResourceModule, MAKEINTRESOURCE(IDR_CONFIG), MAKEINTRESOURCE(RT_TEXT));
		if (!res)
		{
			return configtxt;
		}
		HGLOBAL data = LoadResource(ResourceModule, res);
		if (!data)
		{
			return configtxt;
		}
		size_t size = SizeofResource(ResourceModule, res);
		configtxt = std::string(static_cast<const char*>(LockResource(data)), size);

		return configtxt;
	}

	static bool CheckConfigFile(std::string path)
	{
		FILE* dummy;
		errno_t error = fopen_s(&dummy, path.c_str(), "r");
		if (dummy)
		{
			fclose(dummy);
		}
		return error == 0;
	}
};

HMODULE UConfig::ResourceModule = nullptr;
