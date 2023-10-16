LeCroyUSB
=========

This is a C/C++ port and python wrapper of
[pas-gpib](https://github.com/hansiglaser/pas-gpib/blob/master/usb/usblecroy.pas)'s LeCroy WaveJet driver.
Very simple but the original author's reverse-engineering efforts
saved me a lot of time; thanks!.

[Original Project](https://github.com/hansiglaser/pas-gpib)

[Programming Manual]()

Example
-------

        import pylecroy
        scope=pylecroy.LeCroyUsb()
        # 'chat' sends a command and if it ends with a '?'
        # receives and returns a response string.
        scope.chat("*IDN?")
