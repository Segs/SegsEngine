using System;
using GodotTools.Core;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Xml;
using System.Xml.Linq;
using JetBrains.Annotations;

namespace GodotTools.ProjectEditor
{

    public static class ProjectUtils
    {

        [PublicAPI]
        public static void AddItemToProjectChecked(string projectPath, string itemType, string include)
        {
                // No need to add. It's already included automatically by the MSBuild Sdk.
                // This assumes the source file is inside the project directory and not manually excluded in the csproj
                return;
        }

        public static void RenameItemInProjectChecked(string projectPath, string itemType, string oldInclude, string newInclude)
            {
                // No need to add. It's already included automatically by the MSBuild Sdk.
                // This assumes the source file is inside the project directory and not manually excluded in the csproj
                return;
        }

        public static void RemoveItemFromProjectChecked(string projectPath, string itemType, string include)
            {
                // No need to add. It's already included automatically by the MSBuild Sdk.
                // This assumes the source file is inside the project directory and not manually excluded in the csproj
                return;
        }

        public static void RenameItemsToNewFolderInProjectChecked(string projectPath, string itemType, string oldFolder, string newFolder)
            {
                // No need to add. It's already included automatically by the MSBuild Sdk.
                // This assumes the source file is inside the project directory and not manually excluded in the csproj
                return;
        }

        public static void RemoveItemsInFolderFromProjectChecked(string projectPath, string itemType, string folder)
            {
                // No need to add. It's already included automatically by the MSBuild Sdk.
                // This assumes the source file is inside the project directory and not manually excluded in the csproj
                return;
        }

        private static string[] GetAllFilesRecursive(string rootDirectory, string mask)
        {
            string[] files = Directory.GetFiles(rootDirectory, mask, SearchOption.AllDirectories);

            // We want relative paths
            for (int i = 0; i < files.Length; i++)
            {
                files[i] = files[i].RelativeToPath(rootDirectory);
            }

            return files;
        }

        public static string[] GetIncludeFiles(string projectPath, string itemType)
        {
            return GetAllFilesRecursive(Path.GetDirectoryName(projectPath), "*.cs");
        }
    }
}
