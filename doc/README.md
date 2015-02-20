Novacoin Core 0.5.x
=====================

Setup
---------------------
[Novacoin Core](http://sourceforge.net/projects/novacoin/files/) is the original Novacoin client and it builds the backbone of the network. However, it downloads and stores the entire history of Novacoin transactions (which is currently less than 1 GB); depending on the speed of your computer and network connection, the synchronization process can take anywhere from a few hours to a day or more.

Running
---------------------
The following are some helpful notes on how to run Novacoin on your native platform. 

### Unix

You need the Qt4 run-time libraries to run Novacoin-Qt. On Debian or Ubuntu:

	sudo apt-get install libqtgui4

Unpack the files into a directory and run:

- novacoin-qt (GUI) or novacoind (headless)



### Windows

Unpack the files into a directory, and then run novacoin-qt.exe.

### OSX

Drag Novacoin-Qt to your applications folder, and then run Novacoin-Qt.

### Need Help?

* Ask for help on the [BitcoinTalk](https://bitcointalk.org/) forums, in the [English thread ](https://bitcointalk.org/index.php?topic=143221.0) or [Russian thread](https://bitcointalk.org/index.php?topic=704756.0).

Building
---------------------
The following are developer notes on how to build Novacoin on your native platform. They are not complete guides, but include notes on the necessary libraries, compile flags, etc.

- [OSX Build Notes](build-osx.txt)
- [Unix Build Notes](build-unix.txt)

Development
---------------------
The Bitcoin repo's [root README](https://github.com/bitcoin/bitcoin/blob/master/README.md) contains relevant information on the development process and automated testing.

- [Developer Notes](developer-notes.md)
- [Multiwallet Qt Development](multiwallet-qt.md)
- [Release Notes](release-notes.md)
- [Release Process](release-process.md)
- [Source Code Documentation (External Link)](https://dev.visucore.com/bitcoin/doxygen/)
- [Translation Process](translation_process.md)
- [Unit Tests](unit-tests.md)

### Resources
* Discuss on the [BitcoinTalk](https://bitcointalk.org/) forums, in the [Development & Technical Discussion board](https://bitcointalk.org/index.php?board=6.0).
* Additional info is available [on website](https://novaco.in/).

### Miscellaneous
- [Files](files.md)
- [Tor Support](tor.md)

License
---------------------
See COPYING in root folder.
