[![Godot Engine logo](/logo.png)](https://godotengine.org)

# This is a heavily modified stand-alone fork of the Godot engine

Synced with upstream up to godotengine/godot@99cc4d246f5a9f31fa2e83ad3108013d36dc1965

### Note: Our changes likely introduced new errors, please if at all possible, check similar operations on upstream editor.

## Differences & Goals (incomplete list)

* Codebase is meant to be modernized to a current c++ standard (c++17), but we will be careful with readability ( no `auto`-everything )
* We use Qt5::Core library to ease some of the common tasks, but no Qt types are meant to be a part of the API
* Memory allocation is important, reduce the size of COW object pastures :smile:
* Try to port & clean up most of the internal types to EASTL (Set and Map already done).
* Keep things in sync with upstream ( no more than a 1 week between merges )
* Introduction of 2 types of dynamically/statically loaded plugins : infrastructure ( providing new types, extending engine ) and game ( implementing game logic )
* Reduce the target surface to desktop-like platforms only ( no js, no mobile )
* Remove the death-like grip of gdscript on the engine internals, make it more optional.
* Replace the message/signals with a saner c++ delegates/work queues and expose those to scripting languages.

## Godot Engine

Homepage: https://godotengine.org

#### 2D and 3D cross-platform game engine

Godot Engine is a feature-packed, cross-platform game engine to create 2D and
3D games from a unified interface. It provides a comprehensive set of common
tools, so that users can focus on making games without having to reinvent the
wheel. Games can be exported in one click to a number of platforms, including
the major desktop platforms (Linux, Mac OSX, Windows) as well as mobile
(Android, iOS) and web-based (HTML5) platforms.

#### Free, open source and community-driven

Godot is completely free and open source under the very permissive MIT license.
No strings attached, no royalties, nothing. The users' games are theirs, down
to the last line of engine code. Godot's development is fully independent and
community-driven, empowering users to help shape their engine to match their
expectations. It is supported by the Software Freedom Conservancy
not-for-profit.

Before being open sourced in February 2014, Godot had been developed by Juan
Linietsky and Ariel Manzur (both still maintaining the project) for several
years as an in-house engine, used to publish several work-for-hire titles.

![Screenshot of a 3D scene in Godot Engine](https://raw.githubusercontent.com/godotengine/godot-design/master/screenshots/editor_tps_demo_1920x1080.jpg)

### Getting the engine

#### Binary downloads

Official binaries for the Godot editor and the export templates can be found
[on the homepage](https://godotengine.org/download).

#### Compiling from source

[See the official docs](https://docs.godotengine.org/en/latest/development/compiling/)
for compilation instructions for every supported platform.

### Community and contributing

Godot is not only an engine but an ever-growing community of users and engine
developers. The main community channels are listed [on the homepage](https://godotengine.org/community).

To get in touch with the developers, the best way is to join the
[#godotengine IRC channel](https://webchat.freenode.net/?channels=godotengine)
on Freenode.

To get started contributing to the project, see the [contributing guide](CONTRIBUTING.md).

### Documentation and demos

The official documentation is hosted on [ReadTheDocs](https://docs.godotengine.org).
It is maintained by the Godot community in its own [GitHub repository](https://github.com/godotengine/godot-docs).

The [class reference](https://docs.godotengine.org/en/latest/classes/)
is also accessible from within the engine.

The official demos are maintained in their own [GitHub repository](https://github.com/godotengine/godot-demo-projects)
as well.

There are also a number of other learning resources provided by the community,
such as text and video tutorials, demos, etc. Consult the [community channels](https://godotengine.org/community)
for more info.

[![Travis Build Status](https://travis-ci.org/godotengine/godot.svg?branch=master)](https://travis-ci.org/godotengine/godot)
[![AppVeyor Build Status](https://ci.appveyor.com/api/projects/status/bfiihqq6byxsjxxh/branch/master?svg=true)](https://ci.appveyor.com/project/akien-mga/godot)
[![Code Triagers Badge](https://www.codetriage.com/godotengine/godot/badges/users.svg)](https://www.codetriage.com/godotengine/godot)
[![Translate on Weblate](https://hosted.weblate.org/widgets/godot-engine/-/godot/svg-badge.svg)](https://hosted.weblate.org/engage/godot-engine/?utm_source=widget)
