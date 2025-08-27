.subckt current_mirror_ota is vin_p vin_n gnd vdd
mn0 is is gnd gnd nmos_rvt nfin=20 nf=16 m = 1
mn1 net3 is gnd gnd nmos_rvt nfin=20 nf=16 m = 1

mn2 net1 vin_p net3 gnd nmos_rvt nfin=20 nf=10 m = 2
mn3 net2 vin_m net3 gnd nmos_rvt nfin=20 nf=10 m = 2
 
mp4 net1 net1 vdd vdd nmos_rvt nfin=20 nf=6 m = 1
mp5 net4 net1 vdd vdd nmos_rvt nfin=20 nf=6 m = 1

mp6 net2 net2 vdd vdd pmos_rvt nfin=20 nf=6 m = 1
mp7 vout net2 vdd vdd pmos_rvt nfin=20 nf=6 m = 1

mn8 net4 net4 gnd gnd nmos_rvt nfin=20 nf=6 m = 1
mn9 vout net4 gnd gnd nmos_rvt nfin=20 nf=6 m = 1
.END
