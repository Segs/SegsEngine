using System;
using GodotTools.Core;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Xml;
using System.Xml.Linq;
using JetBrains.Annotations;
using Microsoft.Build.Construction;
using Microsoft.Build.Globbing;

namespace GodotTools.ProjectEditor
{
    public sealed class MSBuildProject
    {
        internal ProjectRootElement Root { get; set; }

        public bool HasUnsavedChanges { get; set; }

        public void Save() => Root.Save();

        public MSBuildProject(ProjectRootElement root)
        {
            Root = root;
        }
    }

    public static class ProjectUtils
    {
        public static MSBuildProject Open(string path)
        {
            var root = ProjectRootElement.Open(path);
            return root != null ? new MSBuildProject(root) : null;
        }

        [PublicAPI]
        public static void AddItemToProjectChecked(string projectPath, string itemType, string include)
        {
            var dir = Directory.GetParent(projectPath).FullName;
            var root = ProjectRootElement.Open(projectPath);
            Debug.Assert(root != null);

            if (root.AreDefaultCompileItemsEnabled())
            {
                // No need to add. It's already included automatically by the MSBuild Sdk.
                // This assumes the source file is inside the project directory and not manually excluded in the csproj
                return;
            }
            var normalizedInclude = include.RelativeToPath(dir).Replace("/", "\\");

            if (root.AddItemChecked(itemType, normalizedInclude))
                root.Save();
        }

        public static void RenameItemInProjectChecked(string projectPath, string itemType, string oldInclude, string newInclude)
        {
            var dir = Directory.GetParent(projectPath).FullName;
            var root = ProjectRootElement.Open(projectPath);
            Debug.Assert(root != null);
            if (root.AreDefaultCompileItemsEnabled())
            {
                // No need to add. It's already included automatically by the MSBuild Sdk.
                // This assumes the source file is inside the project directory and not manually excluded in the csproj
                return;
            }

            var normalizedOldInclude = oldInclude.NormalizePath();
            var normalizedNewInclude = newInclude.NormalizePath();

            var item = root.FindItemOrNullAbs(itemType, normalizedOldInclude);

            if (item == null)
                return;

            item.Include = normalizedNewInclude.RelativeToPath(dir).Replace("/", "\\");
            root.Save();
        }

        public static void RemoveItemFromProjectChecked(string projectPath, string itemType, string include)
        {
            var root = ProjectRootElement.Open(projectPath);
            Debug.Assert(root != null);
            if (root.AreDefaultCompileItemsEnabled())
            {
                // No need to add. It's already included automatically by the MSBuild Sdk.
                // This assumes the source file is inside the project directory and not manually excluded in the csproj
                return;
            }

            var normalizedInclude = include.NormalizePath();

            if (root.RemoveItemChecked(itemType, normalizedInclude))
                root.Save();
        }

        public static void RenameItemsToNewFolderInProjectChecked(string projectPath, string itemType, string oldFolder, string newFolder)
        {
            var dir = Directory.GetParent(projectPath).FullName;
            var root = ProjectRootElement.Open(projectPath);
            Debug.Assert(root != null);
            if (root.AreDefaultCompileItemsEnabled())
            {
                // No need to add. It's already included automatically by the MSBuild Sdk.
                // This assumes the source file is inside the project directory and not manually excluded in the csproj
                return;
            }

            bool dirty = false;

            var oldFolderNormalized = oldFolder.NormalizePath();
            var newFolderNormalized = newFolder.NormalizePath();
            string absOldFolderNormalized = Path.GetFullPath(oldFolderNormalized).NormalizePath();
            string absNewFolderNormalized = Path.GetFullPath(newFolderNormalized).NormalizePath();

            foreach (var item in root.FindAllItemsInFolder(itemType, oldFolderNormalized))
            {
                string absPathNormalized = Path.GetFullPath(item.Include).NormalizePath();
                string absNewIncludeNormalized = absNewFolderNormalized + absPathNormalized.Substring(absOldFolderNormalized.Length);
                item.Include = absNewIncludeNormalized.RelativeToPath(dir).Replace("/", "\\");
                dirty = true;
            }

            if (dirty)
                root.Save();
        }

        public static void RemoveItemsInFolderFromProjectChecked(string projectPath, string itemType, string folder)
        {
            var root = ProjectRootElement.Open(projectPath);
            Debug.Assert(root != null);
            if (root.AreDefaultCompileItemsEnabled())
            {
                // No need to add. It's already included automatically by the MSBuild Sdk.
                // This assumes the source file is inside the project directory and not manually excluded in the csproj
                return;
            }

            var folderNormalized = folder.NormalizePath();

            var itemsToRemove = root.FindAllItemsInFolder(itemType, folderNormalized).ToList();

            if (itemsToRemove.Count > 0)
            {
                foreach (var item in itemsToRemove)
                    item.Parent.RemoveChild(item);

                root.Save();
            }
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
            var result = new List<string>();
            var existingFiles = GetAllFilesRecursive(Path.GetDirectoryName(projectPath), "*.cs");


            var root = ProjectRootElement.Open(projectPath);
            Debug.Assert(root != null);
            if (root.AreDefaultCompileItemsEnabled())
            {
                var excluded = new List<string>();
                result.AddRange(existingFiles);

                foreach (var item in root.Items)
                {
                    if (string.IsNullOrEmpty(item.Condition))
                        continue;

                    if (item.ItemType != itemType)
                        continue;


                    string normalizedRemove= item.Remove.NormalizePath();

                    var glob = MSBuildGlob.Parse(normalizedRemove);

                    excluded.AddRange(result.Where(includedFile => glob.IsMatch(includedFile)));
                }

                result.RemoveAll(f => excluded.Contains(f));
            }

            foreach (var itemGroup in root.ItemGroups)
            {
                if (itemGroup.Condition.Length != 0)
                    continue;

                foreach (var item in itemGroup.Items)
                {
                    if (item.ItemType != itemType)
                        continue;

                    string normalizedInclude = item.Include.NormalizePath();

                    var glob = MSBuildGlob.Parse(normalizedInclude);

                    foreach (var existingFile in existingFiles)
                    {
                        if (glob.IsMatch(existingFile))
                        {
                            result.Add(existingFile);
                        }
                    }
                }
            }

            return result.ToArray();
        }

        public static void EnsureGodotSdkIsUpToDate(MSBuildProject project)
        {
            var root = project.Root;
            string godotSdkAttrValue = ProjectGenerator.GodotSdkAttrValue;

            if (!string.IsNullOrEmpty(root.Sdk) && root.Sdk.Trim().Equals(godotSdkAttrValue, StringComparison.OrdinalIgnoreCase))
                return;

            root.Sdk = godotSdkAttrValue;

            project.HasUnsavedChanges = true;
        }
    }
}
