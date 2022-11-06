using System;
using System.IO;
using System.Text;

namespace GodotTools.ProjectEditor
{
    public static class ProjectGenerator
    {
        public static string GodotSdkAttrValue => "Microsoft.NET.Sdk";
 
        public static string GenAndSaveGameProject(string dir, string name)
        {
            if (name.Length == 0)
                throw new ArgumentException("Project name is empty", nameof(name));

            const string contents = @"
<Project Sdk=""Microsoft.NET.Sdk"">
    <PropertyGroup>
        <TargetFramework>netstandard2.1</TargetFramework>
        <GodotProjectDir Condition=""'$(SolutionDir)' != ''"">$(SolutionDir)</GodotProjectDir>
        <GodotProjectDir Condition=""'$(SolutionDir)' == ''"">$(MSBuildProjectDirectory)</GodotProjectDir>
        <GodotProjectDir>$([MSBuild]::EnsureTrailingSlash('$(GodotProjectDir)'))</GodotProjectDir>
        <BaseOutputPath>$(GodotProjectDir).csharp/temp/bin/</BaseOutputPath>
        <OutputPath>$(GodotProjectDir).csharp/temp/bin/$(Configuration)/</OutputPath>
        <AppendTargetFrameworkToOutputPath>false</AppendTargetFrameworkToOutputPath>
    </PropertyGroup>
    <ItemGroup>
        <Reference Include=""GodotCoreAssembly"">
        <HintPath>$(GodotProjectDir).csharp/assemblies/$(Configuration)/GodotCoreAssembly.dll</HintPath>
        <Private>false</Private>
        </Reference>
        <Reference Include=""GodotEditorAssembly"" Condition="" '$(Configuration)' == 'Debug' "">
        <HintPath>$(GodotProjectDir).csharp/assemblies/$(Configuration)/GodotEditorAssembly.dll</HintPath>
        <Private>false</Private>
        </Reference>
    </ItemGroup>
</Project>";
            const string props = @"
<Project>
  <PropertyGroup>
    <IntermediateOutputPath Condition=""'$(IntermediateOutputPath)' == ''"">$(GodotProjectDir).csharp/temp/obj/</IntermediateOutputPath>
    <BaseIntermediateOutputPath Condition=""'$(BaseIntermediateOutputPath)' == ''"">$(GodotProjectDir).csharp/temp/obj/</BaseIntermediateOutputPath>
  </PropertyGroup>
</Project>
";

            // Save (without BOM)
            using (StreamWriter dir_props = new StreamWriter(Path.Combine(dir,"Directory.Build.props"),false,new UTF8Encoding(false)))
            {
                dir_props.Write(props);
            }

            string path = Path.Combine(dir, name + ".csproj");

            using (StreamWriter cs_proj = new StreamWriter(path,false,new UTF8Encoding(false)))
            {
                cs_proj.Write(contents);
            }

            return Guid.NewGuid().ToString().ToUpper();
        }
    }
}
