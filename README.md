# LumenX Manual ✨ :
## Introduction to LumenX 🗣️ :

- I'm making this work in Windows and Linux, not macOS, because I have no clue how to make anything work in macOS.
- If you want to have it with macOS, then you'll need to make a fork of the repo.
- I will be checking forks, and if someone gets macOS working, I will implement it. (With credit, of course)
- It should work perfectly fine with Visual Studio Code as the IDE by using the `LumenX.code-workspace` as the workspace for this whole thing instead of just using the folder
- I am only testing this on Arch Linux, because I don't like Microsoft so I'm not using Windows. If there is an issue, make an issue report in the repo or better yet, make a fork and do it for me, I as of currently don't have anyone to test Windows Builds on.
- I am thinking about using OpenGL for this project because all GPUs can run this.
- I'm hoping to make this both 3D and 2D (Kinda like Unity) so that you can make as many games as possible.
- I want this to be free and useful for everyone, without requiring people to credit my game engine or make any required payments. (Though some donations would be nice and some credit, but it's all optional)

## Programming Languages, Pros & Cons & Where it's used 🧐:
| Programming Language Used | Where it's used | Positives | Negatives |
| --- | --- | --- | --- |
| C/C++ | Engine | Speedy Engine | Hard to read/use |
| C/C++ | Editor | Complex UI Customization | Limited to just Engine Editor UI |
| HTML | User Created UI (Intended User's game) | It's good for UI Creation (Shown with Websites) | Hard for beginners |
| CSS | User Created Style Sheets for UI (Intended for User's game) | It's pretty darn flexible if you use it correctly | Hard for beginners |

## How it's meant to work 🖥️ :
``` mermaid
%%{init: { 'config': { 'boxMargin': 25, 'actorMargin': 0 } } }%%
sequenceDiagram
    User->>Editor: User Interacts with Editor
    Editor->>Engine: Editor acts as a messenger for the User, Interacting with Engine.
    Engine->>OpenGL Rendering: Gives OpenGL things to render.
    OpenGL Rendering->>Engine: Renders everything for Engine.
    Engine->>Editor: Gives the Editor what it wants (Unless it's code, it will need to be compiled first).
    Editor->>User: Gives User what they want.
    OpenGL Rendering->>User: Gives User Visual Feedback of the output.
```
As you can see, everything should go back and fourth depending on the inputs you give the Editor. Inputs and outputs

## User Instructions 📜 :
In order to build this project (I can only tell you in Visual Studio Code) you will need to have a few things.
1. CMake
2. .Net 10.0
3. GCC/Clang (Any C++ compiler for that matter)
4. Visual Studio Code

Now lets get to seeing some extensions Visual Studio Code will use... nvm, I just realized you should just download whatever the workspace recommends you to. In order to open and get access to the workspace you just need Visual Studio Code, then click File->Open Workspace From File, then select `LumenX.code-workspace` in the file explorer Visual Studio Code will open.

You may also need to input some commands into the terminal.
1. Open Visual Studio Code
2. Open your terminal with ``` Ctl+Shift+` ```
3. Then type in this command inside the terminal
> ```shell
> cd "{PathToLumenX}/Editor/"
> ```
4. Before we do anything else with the terminal, we should open a command panel you'll see at the top of the screen by pressing `Ctl+Shift+P`
5. Click in the command panel and type in
> ```shell
> > CMake: Build
> ```
6. Then type in this command in the terminal
> ```shell
> dotnet build
> ```
7. Then all you need to do now is run the project!
>```shell
> dotnet run
>```

# I hope you all enjoy the Engine! Best Reguards!
