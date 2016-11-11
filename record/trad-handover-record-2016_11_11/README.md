In `ap13-100dbm-ap2-110dbm.mov.mp4`
## Parameters
AP1 and AP3 transfer power set as 100dBm, while AP2 transfer power set as 110dBm(HIGHER than AP1/AP3).
The distance between the moving STA and AP1 or AP2 is the same.
## Result
The result shows that the moving STA **only associate with AP2**(overloaded).

In `ap123-100dbm.mov.mp4`
## Parameters
All the 3 APs transfer power set as 100 dBm.
The distance between the moving STA and AP1 or AP2 is the same.
## Result
The result shows that the moving STA **only associates with AP2**(overloaded).


In `ap13-100dbm-ap2-110dbm-STA-nearer-to-ap1.mov.mp4`
## Parameters
AP1 and AP3 transfer power set as 100dbm, while AP2 transfer power set as 110dBm(HIGHER than AP1/AP3).
The distance between the moving STA and AP1 is shorter than the distance between the moving STA and AP2.
## Result
The result shows that the moving STA **only associates with AP2**(overloaded).


In `ap123-100dbm-STA-nearer-to-ap1.mov.mp4`
## Parameters
All the 3 APs transfer power set as 100 dBm.
The distance between the moving STA and AP1 is shorter than the distance between the moving STA and AP2.
## Result
The result shows that the moving STA **only associates with AP2**(overloaded).

I don't know why would this happen.

In `ap13-100dbm-ap2-90dbm-STA-nearer-to-ap1.mov.mp4`
## Parameters
AP1 and AP3 transfer power set as 100dbm, while AP2 transfer power set as 90dBm(LOWER than AP1/AP3).
The distance between the moving STA and AP1 is SHORTER than the distance between the moving STA and AP2.
## Result
The result shows that the moving STA **only associates with AP1**.

In `ap13-100dbm-ap2-95dbm-STA-nearer-to-ap1.mov.mp4`
## Parameters
AP1 and AP3 transfer power set as 100dbm, while AP2 transfer power set as 95dBm(LOWER than AP1/AP3).
The distance between the moving STA and AP1 is SHORTER than the distance between the moving STA and AP2.
## Result
The result shows that the moving STA **first associate with AP2 and then associates with AP1**.
