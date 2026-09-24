#include "Render/Shader/ShaderCompiler.h"

#include "Core/Assert.h"
#include "Core/IO/Paths.h"

#include <format>
#include <fstream>
#include <limits>

FShaderCompiler::FShaderCompiler(IShaderFormat& InShaderFormat)
	: ShaderFormat(InShaderFormat)
{
}

FShaderCompilerOutput FShaderCompiler::GetOrCompile(const FShaderKey& Key)
{
	checkf(!Key.SourcePath.empty() && !Key.EntryPoint.empty(), "Shader Compiler Key가 비어 있다.");
	const uint64 KeyHash = HashKey(Key);
	const TArray<uint8> Source = LoadFile(FPaths::ResolveContentPath(Key.SourcePath));
	panicf(!Source.empty(), "Shader 소스 파일이 비어 있다. Path={}", Key.SourcePath);
	const uint64 SourceHash = HashSource(Key, Source);
	const std::filesystem::path CachePath = GetShaderCachePath(Key);

	FShaderCompilerOutput Output;
	if (LoadCookedShader(CachePath, KeyHash, SourceHash, Output))
	{
		return Output;
	}

	Output = ShaderFormat.Compile({ Key, Source });
	panicf(!Output.Bytecode.empty(), "Shader Format이 빈 Bytecode를 반환했다. Path={}, EntryPoint={}", Key.SourcePath, Key.EntryPoint);
	SaveCookedShader(CachePath, KeyHash, SourceHash, Output);
	return Output;
}

// 편집 중인 소스를 캐시 없이 다시 컴파일한다. 실패한 결과는 cooked cache에 기록하지 않는다.
bool FShaderCompiler::TryCompile(const FShaderKey& Key, FShaderCompilerOutput& Output, FString& Diagnostics)
{
	TArray<uint8> Source;
	return TryLoadSource(Key, Source, Diagnostics) && TryCompile(Key, Source, Output, Diagnostics);
}

// Shader Key가 가리키는 소스 파일을 읽고 실패 원인을 반환한다.
bool FShaderCompiler::TryLoadSource(const FShaderKey& Key, TArray<uint8>& Source, FString& Diagnostics)
{
	const std::filesystem::path Path = FPaths::ResolveContentPath(Key.SourcePath);
	std::ifstream Stream(Path, std::ios::binary | std::ios::ate);
	if (!Stream)
	{
		Diagnostics = "Shader 소스 파일을 열 수 없다: " + Key.SourcePath;
		return false;
	}
	const std::streamoff Size = Stream.tellg();
	if (Size <= 0 || static_cast<uint64>(Size) > (std::numeric_limits<SIZE_T>::max)())
	{
		Diagnostics = "Shader 소스 파일 크기가 유효하지 않다: " + Key.SourcePath;
		return false;
	}
	Source.resize(static_cast<SIZE_T>(Size));
	Stream.seekg(0, std::ios::beg);
	if (!Stream.read(reinterpret_cast<char*>(Source.data()), Size))
	{
		Diagnostics = "Shader 소스 파일을 읽지 못했다: " + Key.SourcePath;
		return false;
	}
	return true;
}

// 전달된 소스 바이트를 캐시 없이 컴파일하고 성공한 결과만 Output에 저장한다.
bool FShaderCompiler::TryCompile(const FShaderKey& Key, std::span<const uint8> Source, FShaderCompilerOutput& Output, FString& Diagnostics)
{
	FShaderCompilerOutput Compiled;
	if (!ShaderFormat.TryCompile({ Key, Source }, Compiled, Diagnostics) || Compiled.Bytecode.empty())
	{
		if (Diagnostics.empty())
		{
			Diagnostics = "Shader Format이 빈 Bytecode를 반환했다: " + Key.SourcePath;
		}
		return false;
	}
	Output = std::move(Compiled);
	return true;
}

TArray<uint8> FShaderCompiler::LoadFile(const std::filesystem::path& FilePath)
{
	std::ifstream Stream(FilePath, std::ios::binary | std::ios::ate);
	panicf(Stream, "파일을 찾지 못했다. Path={}", FPaths::ToUtf8(FilePath.generic_wstring()));

	const std::streamoff FileSize = Stream.tellg();
	panicf(FileSize >= 0 && static_cast<uint64>(FileSize) <= (std::numeric_limits<SIZE_T>::max)(),
		"파일 크기가 유효하지 않다. Path={}", FPaths::ToUtf8(FilePath.generic_wstring()));

	TArray<uint8> Bytes(static_cast<SIZE_T>(FileSize));
	Stream.seekg(0, std::ios::beg);
	panicf(Bytes.empty() || Stream.read(reinterpret_cast<char*>(Bytes.data()), FileSize),
		"파일을 읽지 못했다. Path={}", FPaths::ToUtf8(FilePath.generic_wstring()));
	return Bytes;
}

uint64 FShaderCompiler::HashKey(const FShaderKey& Key) const
{
	static constexpr uint64 OffsetBasis = 14695981039346656037ull;
	static constexpr uint64 Prime = 1099511628211ull;
	uint64 Hash = OffsetBasis;
	const auto Append = [&Hash](std::span<const uint8> Bytes)
	{
		for (const uint8 Byte : Bytes)
		{
			Hash ^= Byte;
			Hash *= Prime;
		}
	};
	Append({ reinterpret_cast<const uint8*>(Key.SourcePath.data()), Key.SourcePath.size() });
	Append({ reinterpret_cast<const uint8*>(Key.EntryPoint.data()), Key.EntryPoint.size() });
	Append({ reinterpret_cast<const uint8*>(&Key.Stage), sizeof(Key.Stage) });
	Append({ reinterpret_cast<const uint8*>(&Key.PermutationId), sizeof(Key.PermutationId) });
	const FString FormatName = ShaderFormat.GetName().ToString();
	Append({ reinterpret_cast<const uint8*>(FormatName.data()), FormatName.size() });
	const uint32 FormatVersion = ShaderFormat.GetVersion();
	Append({ reinterpret_cast<const uint8*>(&FormatVersion), sizeof(FormatVersion) });
	return Hash;
}

uint64 FShaderCompiler::HashSource(const FShaderKey& Key, std::span<const uint8> Source) const
{
	static constexpr uint64 Prime = 1099511628211ull;
	uint64 Hash = HashKey(Key);
	for (const uint8 Byte : Source)
	{
		Hash ^= Byte;
		Hash *= Prime;
	}
	return Hash;
}

FString FShaderCompiler::GetShaderAssetFileName(const FShaderKey& Key) const
{
	const std::filesystem::path SourcePath(FPaths::ToWide(Key.SourcePath));
	const FString StageName = Key.Stage == EShaderStage::Vertex ? "VS" : "PS";
#if defined(KNOT_BUILD_DEBUG)
	static constexpr const char* BuildName = "Debug";
#elif defined(KNOT_BUILD_DEVELOPMENT)
	static constexpr const char* BuildName = "Development";
#else
	static constexpr const char* BuildName = "Shipping";
#endif
	return std::format("{}_{}_{}_{}_{}_{:016x}.kasset", FPaths::ToUtf8(SourcePath.stem().generic_wstring()),
		Key.EntryPoint, StageName, Key.PermutationId, BuildName, HashKey(Key));
}

std::filesystem::path FShaderCompiler::GetShaderCachePath(const FShaderKey& Key) const
{
	const std::filesystem::path FormatDirectory(FPaths::ToWide(ShaderFormat.GetName().ToString()));
	return (std::filesystem::path(FPaths::SavedDir()) / L"ShaderCache" / FormatDirectory /
		FPaths::ToWide(GetShaderAssetFileName(Key))).lexically_normal();
}

bool FShaderCompiler::LoadCookedShader(
	const std::filesystem::path& FilePath,
	uint64 KeyHash,
	uint64 SourceHash,
	FShaderCompilerOutput& OutOutput)
{
	std::ifstream Stream(FilePath, std::ios::binary);
	if (!Stream)
	{
		return false;
	}

	static constexpr uint32 Magic = 0x4448534B; // "KSHD": Knot Shader Asset
	static constexpr uint32 Version = 1;
	static constexpr uint32 MaxReflectionEntryCount = 65536;
	uint32 FileMagic = 0;
	uint32 FileVersion = 0;
	uint64 FileKeyHash = 0;
	uint64 FileSourceHash = 0;
	uint32 BytecodeSize = 0;
	uint32 ConstantBufferCount = 0;
	uint32 ResourceCount = 0;
	const auto Read = [&Stream](auto& Value)
	{
		return static_cast<bool>(Stream.read(reinterpret_cast<char*>(&Value), sizeof(Value)));
	};
	if (!Read(FileMagic) || !Read(FileVersion) || !Read(FileKeyHash) || !Read(FileSourceHash) ||
		!Read(BytecodeSize) || !Read(ConstantBufferCount) || !Read(ResourceCount) || FileMagic != Magic || FileVersion != Version ||
		FileKeyHash != KeyHash || (SourceHash != 0 && FileSourceHash != SourceHash) || BytecodeSize == 0 ||
		ConstantBufferCount > MaxReflectionEntryCount || ResourceCount > MaxReflectionEntryCount)
	{
		return false;
	}

	const auto ReadName = [&Stream, &Read](FName& OutName)
	{
		uint32 Length = 0;
		if (!Read(Length) || Length == 0 || Length > 4096)
		{
			return false;
		}
		FString Name(Length, '\0');
		if (!Stream.read(Name.data(), Length))
		{
			return false;
		}
		OutName = FName(Name);
		return true;
	};

	FShaderCompilerOutput Output;
	Output.Bytecode.resize(BytecodeSize);
	if (!Stream.read(reinterpret_cast<char*>(Output.Bytecode.data()), BytecodeSize))
	{
		return false;
	}
	Output.Reflection.ConstantBuffers.reserve(ConstantBufferCount);
	for (uint32 BufferIndex = 0; BufferIndex < ConstantBufferCount; ++BufferIndex)
	{
		FShaderConstantBufferDesc Buffer;
		uint8 Stage = 0;
		uint32 ParameterCount = 0;
		if (!ReadName(Buffer.Name) || !Read(Stage) || !Read(Buffer.Slot) || !Read(Buffer.Size) || !Read(ParameterCount) ||
			ParameterCount > MaxReflectionEntryCount)
		{
			return false;
		}
		Buffer.Stage = static_cast<EShaderStage>(Stage);
		Buffer.Parameters.reserve(ParameterCount);
		for (uint32 ParameterIndex = 0; ParameterIndex < ParameterCount; ++ParameterIndex)
		{
			FShaderParameterDesc Parameter;
			uint8 BaseType = 0;
			uint8 Class = 0;
			if (!ReadName(Parameter.Name) || !Read(BaseType) || !Read(Class) || !Read(Parameter.Offset) || !Read(Parameter.Size) ||
				!Read(Parameter.Rows) || !Read(Parameter.Columns) || !Read(Parameter.Elements))
			{
				return false;
			}
			Parameter.BaseType = static_cast<EShaderParameterBaseType>(BaseType);
			Parameter.Class = static_cast<EShaderParameterClass>(Class);
			Buffer.Parameters.push_back(std::move(Parameter));
		}
		Output.Reflection.ConstantBuffers.push_back(std::move(Buffer));
	}
	Output.Reflection.Resources.reserve(ResourceCount);
	for (uint32 ResourceIndex = 0; ResourceIndex < ResourceCount; ++ResourceIndex)
	{
		FShaderResourceBindingDesc Resource;
		uint8 Stage = 0;
		uint8 Type = 0;
		if (!ReadName(Resource.Name) || !Read(Stage) || !Read(Type) || !Read(Resource.Slot) || !Read(Resource.Count))
		{
			return false;
		}
		Resource.Stage = static_cast<EShaderStage>(Stage);
		Resource.Type = static_cast<EShaderResourceType>(Type);
		Output.Reflection.Resources.push_back(std::move(Resource));
	}
	if (Stream.peek() != std::ifstream::traits_type::eof())
	{
		return false;
	}
	OutOutput = std::move(Output);
	return true;
}

void FShaderCompiler::SaveCookedShader(
	const std::filesystem::path& FilePath,
	uint64 KeyHash,
	uint64 SourceHash,
	const FShaderCompilerOutput& Output)
{
	static constexpr uint32 Magic = 0x4448534B; // "KSHD": Knot Shader Asset
	static constexpr uint32 Version = 1;
	panicf(Output.Bytecode.size() <= (std::numeric_limits<uint32>::max)(), "Shader Bytecode가 .kasset 크기 범위를 초과했다.");
	panicf(Output.Reflection.ConstantBuffers.size() <= (std::numeric_limits<uint32>::max)() &&
		Output.Reflection.Resources.size() <= (std::numeric_limits<uint32>::max)(), "Shader Reflection이 .kasset 개수 범위를 초과했다.");

	std::filesystem::create_directories(FilePath.parent_path());
	std::ofstream Stream(FilePath, std::ios::binary | std::ios::trunc);
	panicf(Stream, "cooked Shader Asset을 생성하지 못했다. Path={}", FPaths::ToUtf8(FilePath.generic_wstring()));
	const auto Write = [&Stream](const auto& Value)
	{
		Stream.write(reinterpret_cast<const char*>(&Value), sizeof(Value));
	};
	const auto WriteName = [&Stream, &Write](const FName& Name)
	{
		const FString Text = Name.ToString();
		panicf(!Text.empty() && Text.size() <= (std::numeric_limits<uint32>::max)(), "Shader Reflection Name이 유효하지 않다.");
		const uint32 Length = static_cast<uint32>(Text.size());
		Write(Length);
		Stream.write(Text.data(), Length);
	};

	const uint32 BytecodeSize = static_cast<uint32>(Output.Bytecode.size());
	const uint32 ConstantBufferCount = static_cast<uint32>(Output.Reflection.ConstantBuffers.size());
	const uint32 ResourceCount = static_cast<uint32>(Output.Reflection.Resources.size());
	Write(Magic);
	Write(Version);
	Write(KeyHash);
	Write(SourceHash);
	Write(BytecodeSize);
	Write(ConstantBufferCount);
	Write(ResourceCount);
	Stream.write(reinterpret_cast<const char*>(Output.Bytecode.data()), BytecodeSize);
	for (const FShaderConstantBufferDesc& Buffer : Output.Reflection.ConstantBuffers)
	{
		WriteName(Buffer.Name);
		Write(static_cast<uint8>(Buffer.Stage));
		Write(Buffer.Slot);
		Write(Buffer.Size);
		panicf(Buffer.Parameters.size() <= (std::numeric_limits<uint32>::max)(), "Shader Parameter 개수가 .kasset 범위를 초과했다.");
		Write(static_cast<uint32>(Buffer.Parameters.size()));
		for (const FShaderParameterDesc& Parameter : Buffer.Parameters)
		{
			WriteName(Parameter.Name);
			Write(static_cast<uint8>(Parameter.BaseType));
			Write(static_cast<uint8>(Parameter.Class));
			Write(Parameter.Offset);
			Write(Parameter.Size);
			Write(Parameter.Rows);
			Write(Parameter.Columns);
			Write(Parameter.Elements);
		}
	}
	for (const FShaderResourceBindingDesc& Resource : Output.Reflection.Resources)
	{
		WriteName(Resource.Name);
		Write(static_cast<uint8>(Resource.Stage));
		Write(static_cast<uint8>(Resource.Type));
		Write(Resource.Slot);
		Write(Resource.Count);
	}
	panicf(Stream.good(), "cooked Shader Asset 저장에 실패했다. Path={}", FPaths::ToUtf8(FilePath.generic_wstring()));
}
