using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using GodotTools.Internals;
using Directory = GodotTools.Utils.Directory;
using File = GodotTools.Utils.File;
using OS = GodotTools.Utils.OS;
using Path = System.IO.Path;

namespace GodotTools.Export
{
    public struct AotOptions
    {
        public bool EnableLLVM;
        public bool LLVMOnly;
        public string LLVMPath;
        public string LLVMOutputPath;

        public bool FullAot;

        private bool _useInterpreter;
        public bool UseInterpreter { get => _useInterpreter && !LLVMOnly; set => _useInterpreter = value; }

        public string[] ExtraAotOptions;
        public string[] ExtraOptimizerOptions;

        public string ToolchainPath;
    }

    public static class AotBuilder
    {
        public static void CompileAssemblies(ExportPlugin exporter, AotOptions aotOpts, string[] features, string platform, bool isDebug, string bclDir, string outputDir, string outputDataDir, IDictionary<string, string> assemblies)
        {
            // TODO: WASM

            string aotTempDir = Path.Combine(Path.GetTempPath(), $"godot-aot-{Process.GetCurrentProcess().Id}");

            if (!Directory.Exists(aotTempDir))
                Directory.CreateDirectory(aotTempDir);

            var assembliesPrepared = new Dictionary<string, string>();

            foreach (var dependency in assemblies)
            {
                string assemblyName = dependency.Key;
                string assemblyPath = dependency.Value;

                string assemblyPathInBcl = Path.Combine(bclDir, assemblyName + ".dll");

                if (File.Exists(assemblyPathInBcl))
                {
                    // Don't create teporaries for assemblies from the BCL
                    assembliesPrepared.Add(assemblyName, assemblyPathInBcl);
                }
                else
                {
                    string tempAssemblyPath = Path.Combine(aotTempDir, assemblyName + ".dll");
                    File.Copy(assemblyPath, tempAssemblyPath);
                    assembliesPrepared.Add(assemblyName, tempAssemblyPath);
                }
            }

            string bits = features.Contains("64") ? "64" : features.Contains("32") ? "32" : null;
            CompileAssembliesForDesktop(exporter, platform, isDebug, bits, aotOpts, aotTempDir, outputDataDir, assembliesPrepared, bclDir);
        }

        public static void CompileAssembliesForDesktop(ExportPlugin exporter, string platform, bool isDebug, string bits, AotOptions aotOpts, string aotTempDir, string outputDataDir, IDictionary<string, string> assemblies, string bclDir)
        {
            foreach (var assembly in assemblies)
            {
                string assemblyName = assembly.Key;
                string assemblyPath = assembly.Value;

                string outputFileExtension = platform == OS.Platforms.Windows ? ".dll" :
                    platform == OS.Platforms.OSX ? ".dylib" :
                    ".so";

                string outputFileName = assemblyName + ".dll" + outputFileExtension;
                string tempOutputFilePath = Path.Combine(aotTempDir, outputFileName);

                var compilerArgs = GetAotCompilerArgs(platform, isDebug, bits, aotOpts, assemblyPath, tempOutputFilePath);

                string compilerDirPath = GetMonoCrossDesktopDirName(platform, bits);

                ExecuteCompiler(FindCrossCompiler(compilerDirPath), compilerArgs, bclDir);

                if (platform == OS.Platforms.OSX)
                {
                    exporter.AddSharedObject(tempOutputFilePath,  null);
                }
                else
                {
                    string outputDataLibDir = Path.Combine(outputDataDir, "Mono", platform == OS.Platforms.Windows ? "bin" : "lib");
                    File.Copy(tempOutputFilePath, Path.Combine(outputDataLibDir, outputFileName));
                }
            }
        }

        /// Converts an assembly name to a valid symbol name in the same way the AOT compiler does
        private static string AssemblyNameToAotSymbol(string assemblyName)
        {
            var builder = new StringBuilder();

            foreach (var charByte in Encoding.UTF8.GetBytes(assemblyName))
            {
                char @char = (char)charByte;
                builder.Append(Char.IsLetterOrDigit(@char) || @char == '_' ? @char : '_');
            }

            return builder.ToString();
        }

        private static IEnumerable<string> GetAotCompilerArgs(string platform, bool isDebug, string target, AotOptions aotOpts, string assemblyPath, string outputFilePath)
        {
            // TODO: LLVM

            bool aotSoftDebug = isDebug && !aotOpts.EnableLLVM;
            bool aotDwarfDebug = false; //platform == OS.Platforms.iOS

            var aotOptions = new List<string>();
            var optimizerOptions = new List<string>();

            if (aotOpts.LLVMOnly)
            {
                aotOptions.Add("llvmonly");
            }
            else
            {
                // Can be both 'interp' and 'full'
                if (aotOpts.UseInterpreter)
                    aotOptions.Add("interp");
                if (aotOpts.FullAot)
                    aotOptions.Add("full");
            }

            aotOptions.Add(aotSoftDebug ? "soft-debug" : "nodebug");

            if (aotDwarfDebug)
                aotOptions.Add("dwarfdebug");

            aotOptions.Add($"outfile={outputFilePath}");

            if (aotOpts.EnableLLVM)
            {
                aotOptions.Add($"llvm-path={aotOpts.LLVMPath}");
                aotOptions.Add($"llvm-outfile={aotOpts.LLVMOutputPath}");
            }

            if (aotOpts.ExtraAotOptions.Length > 0)
                aotOptions.AddRange(aotOpts.ExtraAotOptions);

            if (aotOpts.ExtraOptimizerOptions.Length > 0)
                optimizerOptions.AddRange(aotOpts.ExtraOptimizerOptions);

            string EscapeOption(string option) => option.Contains(',') ? $"\"{option}\"" : option;
            string OptionsToString(IEnumerable<string> options) => string.Join(",", options.Select(EscapeOption));

            var runtimeArgs = new List<string>();

            // The '--debug' runtime option is required when using the 'soft-debug' and 'dwarfdebug' AOT options
            if (aotSoftDebug || aotDwarfDebug)
                runtimeArgs.Add("--debug");

            if (aotOpts.EnableLLVM)
                runtimeArgs.Add("--llvm");

            runtimeArgs.Add(aotOptions.Count > 0 ? $"--aot={OptionsToString(aotOptions)}" : "--aot");

            if (optimizerOptions.Count > 0)
                runtimeArgs.Add($"-O={OptionsToString(optimizerOptions)}");

            runtimeArgs.Add(assemblyPath);

            return runtimeArgs;
        }

        private static void ExecuteCompiler(string compiler, IEnumerable<string> compilerArgs, string bclDir)
        {
            // TODO: Once we move to .NET Standard 2.1 we can use ProcessStartInfo.ArgumentList instead
            string CmdLineArgsToString(IEnumerable<string> args)
            {
                // Not perfect, but as long as we are careful...
                return string.Join(" ", args.Select(arg => arg.Contains(" ") ? $@"""{arg}""" : arg));
            }

            using (var process = new Process())
            {
                process.StartInfo = new ProcessStartInfo(compiler, CmdLineArgsToString(compilerArgs))
                {
                    UseShellExecute = false
                };

                process.StartInfo.EnvironmentVariables.Remove("MONO_ENV_OPTIONS");
                process.StartInfo.EnvironmentVariables.Remove("MONO_THREADS_SUSPEND");
                process.StartInfo.EnvironmentVariables.Add("MONO_PATH", bclDir);

                Console.WriteLine($"Running: \"{process.StartInfo.FileName}\" {process.StartInfo.Arguments}");

                if (!process.Start())
                    throw new Exception("Failed to start process for Mono AOT compiler");

                process.WaitForExit();

                if (process.ExitCode != 0)
                    throw new Exception($"Mono AOT compiler exited with code: {process.ExitCode}");
            }
        }

        private static string GetMonoCrossDesktopDirName(string platform, string bits)
        {
            switch (platform)
            {
                case OS.Platforms.Windows:
                    {
                        string arch = bits == "64" ? "x86_64" : "i686";
                        return $"windows-{arch}";
                    }
                case OS.Platforms.OSX:
                    {
                        Debug.Assert(bits == null || bits == "64");
                        string arch = "x86_64";
                        return $"{platform}-{arch}";
                    }
                case OS.Platforms.X11:
                case OS.Platforms.Server:
                    {
                        string arch = bits == "64" ? "x86_64" : "i686";
                        return $"linux-{arch}";
                    }
                default:
                    throw new NotSupportedException($"Platform not supported: {platform}");
            }
        }

        // TODO: Replace this for a specific path for each platform
        private static string FindCrossCompiler(string monoCrossBin)
        {
            string exeExt = OS.IsWindows ? ".exe" : string.Empty;

            var files = new DirectoryInfo(monoCrossBin).GetFiles($"*mono-sgen{exeExt}", SearchOption.TopDirectoryOnly);
            if (files.Length > 0)
                return Path.Combine(monoCrossBin, files[0].Name);

            throw new FileNotFoundException($"Cannot find the mono runtime executable in {monoCrossBin}");
        }
    }
}
