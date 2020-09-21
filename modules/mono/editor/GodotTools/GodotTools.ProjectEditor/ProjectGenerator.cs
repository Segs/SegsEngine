using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using Microsoft.Build.Construction;
using Microsoft.Build.Evaluation;

namespace GodotTools.ProjectEditor
{
    public static class ProjectGenerator
    {
        //public const string GodotSdkVersionToUse = "3.2.3";
        //public static string GodotSdkAttrValue => $"Godot.NET.Sdk/{GodotSdkVersionToUse}";
        public static string GodotSdkAttrValue => "Microsoft.NET.Sdk";

        public static ProjectRootElement GenDirProps()
        {
            // Create directory props to redirect build output... sigh
            var props_root = ProjectRootElement.Create(NewProjectFileOptions.None);
            var propsGroup = props_root.AddPropertyGroup();
            propsGroup.AddProperty("IntermediateOutputPath","$(GodotProjectDir).csharp/temp/obj/").Condition = "'$(IntermediateOutputPath)' == ''"; 
            propsGroup.AddProperty("BaseIntermediateOutputPath","$(GodotProjectDir).csharp/temp/obj/").Condition = "'$(BaseIntermediateOutputPath)' == ''";
            return props_root;

        }
        public static ProjectRootElement GenGameProject(string name)
        {
            if (name.Length == 0)
                throw new ArgumentException("Project name is empty", nameof(name));
            var root = ProjectRootElement.Create(NewProjectFileOptions.None);

            root.Sdk = GodotSdkAttrValue;

            var mainGroup = root.AddPropertyGroup();
            mainGroup.AddProperty("TargetFramework", "net472");
            mainGroup.AddProperty("GodotProjectDir","$(SolutionDir)").Condition = "'$(SolutionDir)' != ''"; 
            mainGroup.AddProperty("GodotProjectDir","$(MSBuildProjectDirectory)").Condition = "'$(SolutionDir)' == ''"; 
            mainGroup.AddProperty("GodotProjectDir","$([MSBuild]::EnsureTrailingSlash('$(GodotProjectDir)'))"); 
            mainGroup.AddProperty("BaseOutputPath","$(GodotProjectDir).csharp/temp/bin/"); 
            mainGroup.AddProperty("OutputPath","$(GodotProjectDir).csharp/temp/bin/$(Configuration)/"); 
            mainGroup.AddProperty("AppendTargetFrameworkToOutputPath","false");
           
            string sanitizedName = IdentifierUtils.SanitizeQualifiedIdentifier(name, allowEmptyIdentifiers: true);

            // If the name is not a valid namespace, manually set RootNamespace to a sanitized one.
            if (sanitizedName != name)
                mainGroup.AddProperty("RootNamespace", sanitizedName);

            var importGroup = root.AddItemGroup();
            var entries = new Dictionary<string, string>();
            entries["HintPath"] = "$(GodotProjectDir).csharp/assemblies/$(Configuration)/GodotCoreAssembly.dll";
            entries["Private"] = "false";
            var item_info=importGroup.AddItem("Reference","GodotCoreAssembly", entries);
            
            var edit_entries = new Dictionary<string, string>();
            edit_entries["HintPath"] = "$(GodotProjectDir).csharp/assemblies/$(Configuration)/GodotEditorAssembly.dll";
            edit_entries["Private"] = "false";
            var item_editor_info=importGroup.AddItem("Reference","GodotEditorAssembly", edit_entries);
            item_editor_info.Condition = " '$(Configuration)' == 'Debug' ";

            // Since we have net472 as target framework, reference the needed package.
            var fw_info=importGroup.AddItem("PackageReference","Microsoft.NETFramework.ReferenceAssemblies");
            fw_info.AddMetadata("Version","1.0.0").ExpressedAsAttribute = true;
            fw_info.AddMetadata("PrivateAssets","All").ExpressedAsAttribute = true;

            return root;
        }

        public static string GenAndSaveGameProject(string dir, string name)
        {
            if (name.Length == 0)
                throw new ArgumentException("Project name is empty", nameof(name));

            string path = Path.Combine(dir, name + ".csproj");

            var props = GenDirProps();

            // Save (without BOM)
            props.Save(Path.Combine(dir,"Directory.Build.props"), new UTF8Encoding(encoderShouldEmitUTF8Identifier: false));

            var root = GenGameProject(name);

            // Save (without BOM)
            root.Save(path, new UTF8Encoding(encoderShouldEmitUTF8Identifier: false));

            return Guid.NewGuid().ToString().ToUpper();
        }
    }
}
