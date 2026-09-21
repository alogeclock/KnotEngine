#include "Editor/Setting/EditorSettings.h"

#include "Core/IO/Paths.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>

bool FEditorSettings::Load()
{
	const std::filesystem::path SettingsPath(FPaths::EditorSettingsPath());
	std::ifstream Stream(SettingsPath);
	if (!Stream)
	{
		Reset();
		return Save();
	}

	TStaticArray<float, 4> LoadedLODSteps = LODSteps;
	TStaticArray<bool, 4> bLoadedLODStep = {};
	FString Section;
	FString Line;
	while (std::getline(Stream, Line))
	{
		Line = Trim(Line);
		if (Line.empty() || Line.starts_with(';') || Line.starts_with('#'))
		{
			continue;
		}
		if (Line.front() == '[' && Line.back() == ']')
		{
			Section = Trim(Line.substr(1, Line.size() - 2));
			continue;
		}
		if (Section != "Graphics")
		{
			continue;
		}

		const SIZE_T Separator = Line.find('=');
		if (Separator == FString::npos)
		{
			continue;
		}
		const FString Key = Trim(Line.substr(0, Separator));
		const FString Value = Trim(Line.substr(Separator + 1));
		try
		{
			for (SIZE_T LODIndex = 0; LODIndex < LoadedLODSteps.size(); ++LODIndex)
			{
				if (Key == "LOD" + std::to_string(LODIndex + 1) + "ScreenPercentage")
				{
					LoadedLODSteps[LODIndex] = std::stof(Value) * 0.01f;
					bLoadedLODStep[LODIndex] = true;
					break;
				}
			}
		}
		catch (const std::exception&)
		{
			Stream.close();
			Reset();
			return Save();
		}
	}
	Stream.close();

	if (!IsValidLODSteps(LoadedLODSteps))
	{
		Reset();
		return Save();
	}
	LODSteps = LoadedLODSteps;
	for (const bool bLoaded : bLoadedLODStep)
	{
		if (!bLoaded)
		{
			return Save();
		}
	}
	return true;
}

bool FEditorSettings::Save() const
{
	const std::filesystem::path SettingsPath(FPaths::EditorSettingsPath());
	const std::filesystem::path TemporaryPath = SettingsPath.wstring() + L".tmp";
	std::error_code Error;
	std::filesystem::create_directories(SettingsPath.parent_path(), Error);
	if (Error)
	{
		return false;
	}

	std::ofstream Stream(TemporaryPath, std::ios::trunc);
	if (!Stream)
	{
		return false;
	}
	Stream << "[Graphics]\n" << std::fixed << std::setprecision(2);
	for (SIZE_T LODIndex = 0; LODIndex < LODSteps.size(); ++LODIndex)
	{
		Stream << "LOD" << LODIndex + 1 << "ScreenPercentage=" << LODSteps[LODIndex] * 100.0f << '\n';
	}
	Stream.close();
	if (!Stream)
	{
		return false;
	}

	std::filesystem::remove(SettingsPath, Error);
	Error.clear();
	std::filesystem::rename(TemporaryPath, SettingsPath, Error);
	return !Error;
}

void FEditorSettings::Reset()
{
	LODSteps = { 0.15f, 0.08f, 0.05f, 0.02f };
}

bool FEditorSettings::IsValidLODSteps(const TStaticArray<float, 4>& Steps)
{
	float PreviousStep = 1.0f;
	for (const float Step : Steps)
	{
		if (!std::isfinite(Step) || Step <= 0.0f || Step >= PreviousStep)
		{
			return false;
		}
		PreviousStep = Step;
	}
	return true;
}

FString FEditorSettings::Trim(const FString& Value)
{
	const SIZE_T First = Value.find_first_not_of(" \t\r\n");
	if (First == FString::npos)
	{
		return {};
	}
	const SIZE_T Last = Value.find_last_not_of(" \t\r\n");
	return Value.substr(First, Last - First + 1);
}
