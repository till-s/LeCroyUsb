import pylecroy
lc=pylecroy.LeCroyUsb()
lc.snd("*IDN?")
print(lc.rcv())
