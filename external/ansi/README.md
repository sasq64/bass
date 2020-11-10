Console
=======

A Console takes a reference to a *Terminal* on creation, and holds a grid of tiles. The user can draw
characters and colors to these tiles using varous functions, and then call *refresh()* to have the Console
translate the changes into screen codes that will update the remote screen accordingly.

Example
-------

    auto *console = createLocalConsole();

    // Write commands are written to the terminal directly. They don't need to be *flushed* and they will wrap at the
    // last column and scroll the screen at the last line
    console->write("One line\nNext line");

    vector<int> colors { Console::RED, Console::WHITE, Console::BLUE };

    for(int y=0; y<10; y++) {
        console->setFG(colors[y % 3]);
        console->put(0,y, format("Line #%d", y));
    }

    // Other output commands require a flush
    console->flush();

    // A telnetserver listens for connections on a port and for each connection runs a function in its own thread.
    TelnetServer telnet { 12345 };
    telnet.setOnConnect([&](TelnetServer::Session &session) {

        // Console input will not work with echo turned on, so always turn off
        session.echo(false); 
        auto console = unique_ptr<Console>(new AnsiConsole { session });
        console->write("LOGIN:);

        // getLine blocks until the full line is entered. For asynchronous behavour, use the *LineEditor* class.
        auto name = console->getLine();
    }

V2
==


A `Terminal` represents an end point, a device to which you can write bytes and read bytes. It is assumed the terminal deals with "raw" input
and not line based. A read() should return directly if no characters are available.

A `Console` is a (presumably visible) 2D grid of text with attributes.

A `Protocol` takes a console command and translates it into a stream of bytes intended for a specific Terminal.

Examples

LocalTerminal - The current terminal

TextConsole (needs a Protocol and a Terminal)
SDLConsole (Renders directly to window)







