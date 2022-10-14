using Godot;
using GodotTools.Core;
using GodotTools.Export;
using GodotTools.Utils;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using GodotTools.Build;
using GodotTools.Ides;
using GodotTools.Ides.Rider;
using GodotTools.Internals;
using GodotTools.ProjectEditor;
using JetBrains.Annotations;
using static GodotTools.Internals.Globals;
using File = GodotTools.Utils.File;
using OS = GodotTools.Utils.OS;
using Path = System.IO.Path;

namespace GodotTools
{
    public class GodotSharpEditor : EditorPlugin, ISerializationListener
    {
        private EditorSettings _editorSettings;

        private PopupMenu _menuPopup;

        private AcceptDialog errorDialog;
        private AcceptDialog aboutDialog;
        private CheckBox aboutDialogCheckBox;

        private ToolButton _bottomPanelBtn;
        private ToolButton toolBarBuildButton;

        public GodotIdeManager GodotIdeManager { get; private set; }

        private WeakRef exportPluginWeak; // TODO Use WeakReference once we have proper serialization

        public MSBuildPanel MSBuildPanel { get; private set; }

        public bool SkipBuildBeforePlaying { get; set; } = false;

        public static string ProjectAssemblyName
        {
            get
            {
                var projectAssemblyName = (string)ProjectSettings.GetSetting("application/config/name");
                projectAssemblyName = projectAssemblyName.ToSafeDirName();
                if (string.IsNullOrEmpty(projectAssemblyName))
                    projectAssemblyName = "UnnamedProject";
                return projectAssemblyName;
            }
        }

        private bool CreateProjectSolution()
        {
            using (var pr = new EditorProgress("create_csharp_solution", "Generating solution...".TTR(), 3))
            {
                pr.Step("Generating C# project...".TTR());

                string resourceDir = ProjectSettings.GlobalizePath("res://");

                string path = resourceDir;
                string name = GodotSharpDirs.ProjectAssemblyName;

                string guid = CsProjOperations.GenerateGameProject(path, name);

                if (guid.Length <= 0)
                {
                    ShowErrorDialog("Failed to create C# project.".TTR());
                    return false;
                }

                var solution = new DotNetSolution(name)
                {
                    DirectoryPath = path
                };

                var projectInfo = new DotNetSolution.ProjectInfo
                {
                    Guid = guid,
                    PathRelativeToSolution = name + ".csproj",
                    Configs = new List<string> {"Debug", "ExportDebug", "ExportRelease"}
                };

                solution.AddNewProject(name, projectInfo);

                try
                {
                    solution.Save();
                }
                catch (IOException e)
                {
                    ShowErrorDialog("Failed to save solution. Exception message: ".TTR() + e.Message);
                    return false;
                }

                pr.Step("Updating Godot API assemblies...".TTR());

                string debugApiAssembliesError = Internal.UpdateApiAssembliesFromPrebuilt("Debug");

                if (!string.IsNullOrEmpty(debugApiAssembliesError))
                {
                    ShowErrorDialog("Failed to update the Godot API assemblies(Debug): " + debugApiAssembliesError);
                    return false;
                }

                string releaseApiAssembliesError = Internal.UpdateApiAssembliesFromPrebuilt("Release");

                if (!string.IsNullOrEmpty(releaseApiAssembliesError))
                {
                    ShowErrorDialog("Failed to update the Godot API assemblies(Release): " + releaseApiAssembliesError);
                    // NOTE: SEGS: This is not a critical error, so we continue. 
                    //return false;
                }

                pr.Step("Done".TTR());

                // Here, after all calls to progress_task_step
                CallDeferred(nameof(_RemoveCreateSlnMenuOption));

                return true;
            }
        }

        private void _RemoveCreateSlnMenuOption()
        {
            _menuPopup.RemoveItem(_menuPopup.GetItemIndex((int)MenuOptions.CreateSln));
            _bottomPanelBtn.Show();
            toolBarBuildButton.Show();
        }

        private void _ShowAboutDialog()
        {
            bool showOnStart = (bool)_editorSettings.GetSetting("mono/editor/show_info_on_start");
            aboutDialogCheckBox.Pressed = showOnStart;
            aboutDialog.PopupCenteredMinsize();
        }
        private void _ToggleAboutDialogOnStart(bool enabled)
        {
            bool showOnStart = (bool)_editorSettings.GetSetting("mono/editor/show_info_on_start");
            if (showOnStart != enabled)
                _editorSettings.SetSetting("mono/editor/show_info_on_start", enabled);
        }

        private void _MenuOptionPressed(int id)
        {
            switch ((MenuOptions)id)
            {
                case MenuOptions.CreateSln:
                    CreateProjectSolution();
                    break;
                case MenuOptions.AboutCSharp:
                    _ShowAboutDialog();
                    break;
                default:
                    throw new ArgumentOutOfRangeException(nameof(id), id, "Invalid menu option");
            }
        }

        private void _BuildSolutionPressed()
        {
            if (!File.Exists(GodotSharpDirs.ProjectSlnPath))
            {
                if (!CreateProjectSolution())
                    return; // Failed to create solution
            }

            Instance.MSBuildPanel.BuildSolution();
        }
        private void _FileSystemDockFileMoved(string file, string newFile)
        {
            if (Path.GetExtension(file) == Internal.CSharpLanguageExtension)
            {
                ProjectUtils.RenameItemInProjectChecked(GodotSharpDirs.ProjectCsProjPath, "Compile",
                    ProjectSettings.GlobalizePath(file), ProjectSettings.GlobalizePath(newFile));
            }
        }

        private void _FileSystemDockFileRemoved(string file)
        {
            if (Path.GetExtension(file) == Internal.CSharpLanguageExtension) 
            {
                ProjectUtils.RemoveItemFromProjectChecked(GodotSharpDirs.ProjectCsProjPath, "Compile",
                    ProjectSettings.GlobalizePath(file));
            }
        }

        private void _FileSystemDockFolderMoved(string oldFolder, string newFolder)
        {
            if (File.Exists(GodotSharpDirs.ProjectCsProjPath))
            {
                ProjectUtils.RenameItemsToNewFolderInProjectChecked(GodotSharpDirs.ProjectCsProjPath, "Compile",
                    ProjectSettings.GlobalizePath(oldFolder), ProjectSettings.GlobalizePath(newFolder));
            }
        }

        private void _FileSystemDockFolderRemoved(string oldFolder)
        {
            if (File.Exists(GodotSharpDirs.ProjectCsProjPath))
            {
                ProjectUtils.RemoveItemsInFolderFromProjectChecked(GodotSharpDirs.ProjectCsProjPath, "Compile",
                    ProjectSettings.GlobalizePath(oldFolder));
            }
        }

        public override void _Ready()
            {
            base._Ready();
            MSBuildPanel.BuildOutputView.BuildStateChanged +=BuildStateChanged;
            bool showInfoDialog = (bool)_editorSettings.GetSetting("mono/editor/show_info_on_start");
            if (showInfoDialog)
            {
                aboutDialog.PopupMode.Exclusive = true;
                _ShowAboutDialog();
                // Once shown a first time, it can be seen again via the Mono menu - it doesn't have to be exclusive from that time on.
                aboutDialog.PopupMode.Exclusive = false;
            }
            var fileSystemDock = GetEditorInterface().GetFileSystemDock();
            fileSystemDock.FilesMoved += _FileSystemDockFileMoved;
            fileSystemDock.FileRemoved += _FileSystemDockFileRemoved;
            fileSystemDock.FolderMoved += _FileSystemDockFolderMoved;
            fileSystemDock.FolderRemoved +=_FileSystemDockFolderRemoved;
        }

        private enum MenuOptions
        {
            CreateSln,
            AboutCSharp,
        }

        public void ShowErrorDialog(string message, string title = "Error")
        {
            errorDialog.WindowTitle = title;
            errorDialog.Dialog._Text = message;
            errorDialog.PopupCenteredMinsize();
        }

        private static string _vsCodePath = string.Empty;

        private static readonly string[] VsCodeNames =
        {
            "code", "code-oss", "vscode", "vscode-oss", "visual-studio-code", "visual-studio-code-oss"
        };

        [UsedImplicitly]
        public Error OpenInExternalEditor(Script script, int line, int col)
        {
            var editorId = (ExternalEditorId)_editorSettings.GetSetting("mono/editor/external_editor");

            switch (editorId)
            {
                case ExternalEditorId.None:
                    // Not an error. Tells the caller to fallback to the global external editor settings or the built-in editor.
                    return Error.Unavailable;
                case ExternalEditorId.VisualStudio:
                {
                    string scriptPath = ProjectSettings.GlobalizePath(script.Resource_.Path);

                    var args = new List<string>
                    {
                        GodotSharpDirs.ProjectSlnPath,
                        line >= 0 ? $"{scriptPath};{line + 1};{col + 1}" : scriptPath
                    };

                    string command = Path.Combine(GodotSharpDirs.DataEditorToolsDir, "GodotTools.OpenVisualStudio.exe");

                    try
                    {
                        if (Godot.OS.IsStdoutVerbose())
                            Console.WriteLine($"Running: \"{command}\" {string.Join(" ", args.Select(a => $"\"{a}\""))}");

                        OS.RunProcess(command, args);
                    }
                    catch (Exception e)
                    {
                        GD.PushError($"Error when trying to run code editor: VisualStudio. Exception message: '{e.Message}'");
                    }

                    break;
                }
                case ExternalEditorId.VisualStudioForMac:
                    goto case ExternalEditorId.MonoDevelop;
                case ExternalEditorId.Rider:
                {
                    string scriptPath = ProjectSettings.GlobalizePath(script.Resource_.Path);
                    RiderPathManager.OpenFile(GodotSharpDirs.ProjectSlnPath, scriptPath, line);
                    return Error.Ok;
                }
                case ExternalEditorId.MonoDevelop:
                {
                    string scriptPath = ProjectSettings.GlobalizePath(script.Resource_.Path);

                    GodotIdeManager.LaunchIdeAsync().ContinueWith(launchTask =>
                    {
                        var editorPick = launchTask.Result;
                        if (line >= 0)
                            editorPick?.SendOpenFile(scriptPath, line + 1, col);
                        else
                            editorPick?.SendOpenFile(scriptPath);
                    });

                    break;
                }

                case ExternalEditorId.VsCode:
                {
                    if (string.IsNullOrEmpty(_vsCodePath) || !File.Exists(_vsCodePath))
                    {
                        // Try to search it again if it wasn't found last time or if it was removed from its location
                        _vsCodePath = VsCodeNames.SelectFirstNotNull(OS.PathWhich, orElse: string.Empty);
                    }

                    var args = new List<string>();

                    bool osxAppBundleInstalled = false;

                    if (OS.IsOSX)
                    {
                        // The package path is '/Applications/Visual Studio Code.app'
                        const string vscodeBundleId = "com.microsoft.VSCode";

                        osxAppBundleInstalled = Internal.IsOsxAppBundleInstalled(vscodeBundleId);

                        if (osxAppBundleInstalled)
                        {
                            args.Add("-b");
                            args.Add(vscodeBundleId);

                            // The reusing of existing windows made by the 'open' command might not choose a wubdiw that is
                            // editing our folder. It's better to ask for a new window and let VSCode do the window management.
                            args.Add("-n");

                            // The open process must wait until the application finishes (which is instant in VSCode's case)
                            args.Add("--wait-apps");

                            args.Add("--args");
                        }
                    }

                    var resourcePath = ProjectSettings.GlobalizePath("res://");
                    args.Add(resourcePath);

                    string scriptPath = ProjectSettings.GlobalizePath(script.Resource_.Path);

                    if (line >= 0)
                    {
                        args.Add("-g");
                        args.Add($"{scriptPath}:{line}:{col}");
                    }
                    else
                    {
                        args.Add(scriptPath);
                    }

                    string command;

                    if (OS.IsOSX)
                    {
                        if (!osxAppBundleInstalled && string.IsNullOrEmpty(_vsCodePath))
                        {
                            GD.PushError("Cannot find code editor: VSCode");
                            return Error.FileNotFound;
                        }

                        command = osxAppBundleInstalled ? "/usr/bin/open" : _vsCodePath;
                    }
                    else
                    {
                        if (string.IsNullOrEmpty(_vsCodePath))
                        {
                            GD.PushError("Cannot find code editor: VSCode");
                            return Error.FileNotFound;
                        }

                        command = _vsCodePath;
                    }

                    try
                    {
                        OS.RunProcess(command, args);
                    }
                    catch (Exception e)
                    {
                        GD.PushError($"Error when trying to run code editor: VSCode. Exception message: '{e.Message}'");
                    }

                    break;
                }

                default:
                    throw new ArgumentOutOfRangeException();
            }

            return Error.Ok;
        }

        [UsedImplicitly]
        public bool OverridesExternalEditor()
        {
            return (ExternalEditorId)_editorSettings.GetSetting("mono/editor/external_editor") != ExternalEditorId.None;
        }

        public override bool Build()
        {
            return BuildManager.EditorBuildCallback();
        }


        private void BuildStateChanged()
        {
            if (_bottomPanelBtn != null)
                _bottomPanelBtn.Icon = MSBuildPanel.BuildOutputView.BuildStateIcon;
        }

        public override void EnablePlugin()
        {
            base.EnablePlugin();

            if (Instance != null)
                throw new InvalidOperationException();
            Instance = this;

            var editorInterface = GetEditorInterface();
            var editorBaseControl = editorInterface.GetBaseControl();

            _editorSettings = editorInterface.GetEditorSettings();

            errorDialog = new AcceptDialog();
            editorBaseControl.AddChild(errorDialog);

            MSBuildPanel = new MSBuildPanel();
            _bottomPanelBtn = AddControlToBottomPanel(MSBuildPanel, "MSBuild".TTR());

            AddChild(new HotReloadAssemblyWatcher {Name = "HotReloadAssemblyWatcher"});

            _menuPopup = new PopupMenu();
            _menuPopup.Hide();
            _menuPopup.SetAsTopLevel(true);

            AddToolSubmenuItem("C#", _menuPopup);

            // TODO: Remove or edit this info dialog once Mono support is no longer in alpha
            {
                _menuPopup.AddItem("About C# support".TTR(), (int)MenuOptions.AboutCSharp);
                aboutDialog = new AcceptDialog();
                editorBaseControl.AddChild(aboutDialog);
                aboutDialog.WindowTitle = "Important: C# support is not feature-complete";

                // We don't use DialogText as the default AcceptDialog Label doesn't play well with the TextureRect and CheckBox
                // we'll add. Instead we add containers and a new autowrapped Label inside.

                // Main VBoxContainer (icon + label on top, checkbox at bottom)
                var aboutVBox = new VBoxContainer();
                aboutDialog.AddChild(aboutVBox);

                // HBoxContainer for icon + label
                var aboutHBox = new HBoxContainer();
                aboutVBox.AddChild(aboutHBox);

                var aboutIcon = new TextureRect();
                aboutIcon.Texture = aboutIcon.GetThemeIcon("NodeWarning", "EditorIcons");
                aboutHBox.AddChild(aboutIcon);

                var aboutLabel = new Label();
                aboutHBox.AddChild(aboutLabel);

                aboutLabel.Rect.MinSize = new Vector2(600, 150) * EditorScale;
                aboutLabel.SizeFlags.Vertical = (int) Control.SizeFlagsEnum.ExpandFill;
                aboutLabel.Autowrap = true;
                aboutLabel.Text = "C# support in Godot Engine is in late alpha stage and, while already usable, " +
                                  "it is not meant for use in production.\n\n" +
                                  "Projects can be exported to Linux, macOS, Windows. " +
                                  "Bugs and usability issues will be addressed gradually over future releases, " +
                                  "potentially including compatibility breaking changes as new features are implemented for a better overall C# experience.\n\n" +
                                  "If you experience issues with this Mono build, please report them on Godot's issue tracker with details about your system, MSBuild version, IDE, etc.:\n\n" +
                                  "        https://github.com/godotengine/godot/issues\n\n" +
                                  "Your critical feedback at this stage will play a great role in shaping the C# support in future releases, so thank you!";

                EditorDef("mono/editor/show_info_on_start", true);

                // CheckBox in main container
                aboutDialogCheckBox = new CheckBox {Text = "Show this warning when starting the editor"};
                aboutDialogCheckBox.Toggled += _ToggleAboutDialogOnStart;
                aboutVBox.AddChild(aboutDialogCheckBox);
            }

            toolBarBuildButton = new ToolButton
            {
                Text = "Build",
                Hint = { Tooltip = "Build solution" },
                Focus = { Mode = Control.FocusMode.None }
            };
            toolBarBuildButton.PressedSignal += _BuildSolutionPressed;
            AddControlToContainer(CustomControlContainer.Toolbar, toolBarBuildButton);
            if (!File.Exists(GodotSharpDirs.ProjectSlnPath) || !File.Exists(GodotSharpDirs.ProjectCsProjPath))
            {
                _bottomPanelBtn.Hide();
                toolBarBuildButton.Hide();
                _menuPopup.AddItem("Create C# solution".TTR(), (int)MenuOptions.CreateSln);
            }

            _menuPopup.IdPressed += _MenuOptionPressed;

            // External editor settings
            EditorDef("mono/editor/external_editor", ExternalEditorId.None);

            string settingsHintStr = "Disabled";

            if (OS.IsWindows)
            {
                settingsHintStr += $",Visual Studio:{(int)ExternalEditorId.VisualStudio}" +
                                   $",MonoDevelop:{(int)ExternalEditorId.MonoDevelop}" +
                                   $",Visual Studio Code:{(int)ExternalEditorId.VsCode}" +
                                   $",JetBrains Rider:{(int)ExternalEditorId.Rider}";
            }
            else if (OS.IsOSX)
            {
                settingsHintStr += $",Visual Studio:{(int)ExternalEditorId.VisualStudioForMac}" +
                                   $",MonoDevelop:{(int)ExternalEditorId.MonoDevelop}" +
                                   $",Visual Studio Code:{(int)ExternalEditorId.VsCode}" +
                                   $",JetBrains Rider:{(int)ExternalEditorId.Rider}";
            }
            else if (OS.IsUnixLike)
            {
                settingsHintStr += $",MonoDevelop:{(int)ExternalEditorId.MonoDevelop}" +
                                   $",Visual Studio Code:{(int)ExternalEditorId.VsCode}" +
                                   $",JetBrains Rider:{(int)ExternalEditorId.Rider}";
            }

            _editorSettings.AddPropertyInfo(new Godot.Collections.Dictionary
            {
                ["type"] = VariantType.Int,
                ["name"] = "mono/editor/external_editor",
                ["hint"] = PropertyHint.Enum,
                ["hint_string"] = settingsHintStr
            });

            // Export plugin
            var exportPlugin = new ExportPlugin();
            AddExportPlugin(exportPlugin);
            exportPlugin.RegisterExportSettings();
            exportPluginWeak = WeakRef(exportPlugin);

            BuildManager.Initialize();
            RiderPathManager.Initialize();

            GodotIdeManager = new GodotIdeManager();
            AddChild(GodotIdeManager);
        }

        protected override void Dispose(bool disposing)
        {
            base.Dispose(disposing);

            if (exportPluginWeak != null)
            {
                // We need to dispose our export plugin before the editor destroys EditorSettings.
                // Otherwise, if the GC disposes it at a later time, EditorExportPlatformAndroid
                // will be freed after EditorSettings already was, and its device polling thread
                // will try to access the EditorSettings singleton, resulting in null dereferencing.
                (exportPluginWeak.GetRef() as ExportPlugin)?.Dispose();

                exportPluginWeak.Dispose();
            }

            GodotIdeManager?.Dispose();
        }

        public void OnBeforeSerialize()
        {
        }

        public void OnAfterDeserialize()
        {
            Instance = this;
        }

        // Singleton

        public static GodotSharpEditor Instance { get; private set; }

        [UsedImplicitly]
        private GodotSharpEditor()
        {
        }
    }
}
