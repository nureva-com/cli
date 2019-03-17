/*******************************************************************************
 * CLI - A simple command line interface.
 * Copyright (C) Daniele Pallastrelli
 *
 * Boost Software License - Version 1.0 - August 17th, 2003
 *
 * Permission is hereby granted, free of charge, to any person or organization
 * obtaining a copy of the software and accompanying documentation covered by
 * this license (the "Software") to use, reproduce, display, distribute,
 * execute, and transmit the Software, and to prepare derivative works of the
 * Software, and to permit third-parties to whom the Software is furnished to
 * do so, all subject to the following:
 *
 * The copyright notices in the Software and this entire statement, including
 * the above license grant, this restriction and the following disclaimer,
 * must be included in all copies of the Software, in whole or in part, and
 * all derivative works of the Software, unless such copies or derivative
 * works are solely in the form of machine-executable object code generated by
 * a source language processor.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ******************************************************************************/

#include "cli/clilocalsession.h" // include boost asio
#include "cli/cli.h"
#include <vector>

using namespace cli;
using namespace std;

///////////////////////////////////////////////////////////////////////////////////////////////
// plugin mechanism

class Plugin
{
public:
    Plugin(const string& _name) : name(_name) { cout << "Plugin " << name << " loaded" << endl; }
    virtual ~Plugin() { cout << "Plugin " << name << " unloaded" << endl; }
    const string& Name() const { return name; }
private:
    const string name;
};

using Factory = function<unique_ptr<Plugin>(Menu*)>;

class PluginRegistry
{
public:
    static PluginRegistry& Instance() { return instance; }
    void Register(const string& name, Factory factory) { plugins.push_back(make_pair(name, factory)); }
    void Print(ostream& out) const
    {
        for (auto& p: plugins)
            out << " - " << p.first << endl;
    }
    unique_ptr<Plugin> Create(const string& name, Menu* menu) const
    {
        for (auto& p: plugins)
            if (p.first == name)
                return p.second(menu);
        return {};
    }
private:
    static PluginRegistry instance;
    vector<pair<string, Factory>> plugins;
};

PluginRegistry PluginRegistry::instance;

class PluginContainer
{
public:
    static PluginContainer& Instance() { return instance; }
    void SetMenu(Menu& _menu) { menu = &_menu; }
    void Load(const string& plugin)
    {
        auto p = PluginRegistry::Instance().Create(plugin, menu);
        if (p)
            plugins.push_back(move(p));
    }
    void Unload(const string& plugin)
    {
        plugins.erase(
            remove_if(
                plugins.begin(),
                plugins.end(),
                [&](auto& p){ return p->Name() == plugin; }
            ),
            plugins.end()
        );
    }
    void PrintLoaded(ostream& out) const
    {
        for (auto& p: plugins)
            out << " - " << p->Name() << endl;        
    }
private:
    static PluginContainer instance;
    vector<unique_ptr<Plugin>> plugins;
    Menu* menu;
};

PluginContainer PluginContainer::instance;

class Registration
{
public:
    Registration(const string& name, Factory factory)
    {
        PluginRegistry::Instance().Register(name, factory);
    }
};

template <typename T, const char* NAME>
class RegisteredPlugin : public Plugin
{
public:
    RegisteredPlugin() : Plugin(NAME)
    {
        const Registration& dummy = registration;
    }
private:
    static unique_ptr<Plugin> Create(Menu* menu) { return make_unique<T>(menu); }
    static Registration registration;
};

template <typename T, const char* NAME>
Registration RegisteredPlugin<T, NAME>::registration(NAME, &RegisteredPlugin<T, NAME>::Create);

///////////////////////////////////////////////////////////////////////////////////////////////
// plugins

// plugin Arithmetic

constexpr char ArithmeticName[] = "arithmetic";

class Arithmetic : public RegisteredPlugin<Arithmetic, ArithmeticName>
{
public:
    Arithmetic(Menu* menu)
    {
        auto subMenu = make_unique<Menu>(Name());
        subMenu -> Insert(
                "add", {"first_term", "second_term"},
                [](std::ostream& out, int x, int y)
                {
                    out << x << " + " << y << " = " << (x+y) << "\n";
                },
                "Print the sum of the two numbers" );
        subMenu -> Insert(
                "add",
                [](std::ostream& out, int x, int y, int z)
                {
                    out << x << " + " << y << " + " << z << " = " << (x+y+z) << "\n";
                },
                "Print the sum of the three numbers" );
        subMenu -> Insert(
                "sub", {"subtrahend", "minuend"},
                [](std::ostream& out, int x, int y)
                {
                    out << x << " - " << y << " = " << (x-y) << "\n";
                },
                "Print the result of a subtraction" );

        menuHandler = menu->Insert(move(subMenu));
    }
    ~Arithmetic()
    {
        menuHandler.Remove();
    }
private:
    CmdHandler menuHandler;
};

// plugin Strings

constexpr char StringsName[] = "strings";

class Strings : public RegisteredPlugin<Strings, StringsName>
{
public:
    Strings(Menu* menu)
    {
        auto subMenu = make_unique<Menu>(Name());
        subMenu -> Insert(
                "reverse", {"string_to_revers"},
                [](std::ostream& out, const string& arg)
                {
                    string copy(arg);
                    std::reverse(copy.begin(), copy.end());
                    out << copy << "\n";
                },
                "Print the reverse string" );

        subMenu -> Insert(
                "upper",
                [](std::ostream& out, string arg)
                {
                    std::transform(arg.begin(), arg.end(),arg.begin(), ::toupper);
                    out << arg << "\n";
                },
                "Print the string in uppercase" );
                
        menuHandler = menu->Insert(move(subMenu));
    }
    ~Strings()
    {
        menuHandler.Remove();
    }
private:
    CmdHandler menuHandler;
};

///////////////////////////////////////////////////////////////////////////////////////////////
// Entry point

int main()
{
    boost::asio::io_context ioc;
    CmdHandler colorCmd;
    CmdHandler nocolorCmd;    

    // setup cli

    auto rootMenu = make_unique< Menu >( "cli" );
    PluginContainer::Instance().SetMenu(*rootMenu);
    rootMenu -> Insert(
            "list",
            [](std::ostream& out){ PluginRegistry::Instance().Print(out); },
            "Print the plugin list" );
    rootMenu -> Insert(
            "loaded",
            [](std::ostream& out)
            {
                PluginContainer::Instance().PrintLoaded(out);
            },
            "Load the plugin specified" );
    rootMenu -> Insert(
            "load", {"plugin_name"},
            [](std::ostream& out, const string& plugin)
            {
                PluginContainer::Instance().Load(plugin);
            },
            "Load the plugin specified" );
    rootMenu -> Insert(
            "unload", {"plugin_name"},
            [](std::ostream& out, const string& plugin)
            {
                PluginContainer::Instance().Unload(plugin);
            },
            "Unload the plugin specified" );
    colorCmd = rootMenu -> Insert(
            "color",
            [&](std::ostream& out)
            {
                out << "Colors ON\n";
                SetColor();
                colorCmd.Disable();
                nocolorCmd.Enable();
            },
            "Enable colors in the cli" );
    nocolorCmd = rootMenu -> Insert(
            "nocolor",
            [&](std::ostream& out)
            {
                out << "Colors OFF\n";
                SetNoColor();
                colorCmd.Enable();
                nocolorCmd.Disable();                
            },
            "Disable colors in the cli" );


    Cli cli( std::move(rootMenu) );
    // global exit action
    cli.ExitAction( [](auto& out){ out << "Goodbye and thanks for all the fish.\n"; } );

    CliLocalTerminalSession localSession(cli, ioc, std::cout, 200);
    localSession.ExitAction(
        [&ioc](auto& out) // session exit action
        {
            out << "Closing App...\n";
            ioc.stop();
        }
    );

    auto work = boost::asio::make_work_guard(ioc);
    ioc.run();

    return 0;
}
