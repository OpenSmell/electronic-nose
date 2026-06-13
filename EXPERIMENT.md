# Interoperability Experiment Protocol

**Goal:** Prove that your e-nose produces latent-space encodings consistent with SmellNet's 44-substance prototypes after calibration.

## Prerequisites

- OpenSmell SDK installed (`pip install opensmell`)
- Electronic nose hardware built and flashed
- 2–5 common kitchen substances (garlic, ginger, cinnamon, nutmeg, lemon)

## Quick Test (2 substances)

1. Record garlic for 60 seconds → `garlic.csv`
2. Record ginger for 60 seconds → `ginger.csv`
3. Run calibration:
   ```python
   import numpy as np
   from opensmell.encoder import Encoder
   from opensmell.preprocessing import expand_channels, per_recording_zscore, segment
   from sklearn.decomposition import PCA
   import json

   def encode_file(path):
       raw = np.loadtxt(path, delimiter=",")
       exp = expand_channels(raw)
       normed = per_recording_zscore(exp)
       segs = segment(normed, stride=5)
       enc = Encoder.load_auto()
       latents = enc.encode(segs)
       return latents.mean(axis=0)

   user_g = encode_file("garlic.csv")
   user_i = encode_file("ginger.csv")

   protos = np.load("prototypes.npy")
   with open("prototype_labels.json") as f:
       labels = json.load(f)

   # 2D Procrustes
   pca = PCA(n_components=2).fit(protos)
   g_sn = pca.transform(protos[labels.index("garlic")])[0]
   i_sn = pca.transform(protos[labels.index("ginger")])[0]
   u_g = pca.transform(user_g.reshape(1, -1))[0]
   u_i = pca.transform(user_i.reshape(1, -1))[0]

   A = np.stack([u_g, u_i])
   B = np.stack([g_sn, i_sn])
   U, _, Vt = np.linalg.svd(B.T @ A)
   R = U @ Vt

   print("Calibration complete. R matrix shape:", R.shape)
   ```
4. Record an unknown substance → `unknown.csv`
5. Encode, rotate, and identify:
   ```python
   u_unk = pca.transform(encode_file("unknown.csv").reshape(1, -1))[0]
   u_unk_rot = R @ u_unk
   sims = [np.dot(u_unk_rot, pca.transform(p.reshape(1, -1))[0]) for p in protos]
   print("Predicted:", labels[np.argmax(sims)])
   ```

## Full Validation (5 substances)

Record garlic, ginger, cinnamon, nutmeg, and lemon — all in SmellNet's 44 prototypes.

| Substance | Predicted | Confidence | Notes |
|-----------|-----------|------------|-------|
| Garlic | | | |
| Ginger | | | |
| Cinnamon | | | |
| Nutmeg | | | |
| Lemon | | | |

**Success criterion:** At least 4/5 correct with confidence > 0.7.

## Calibration tips

- Properly fill the calibration set: 2 substances constrain a 2D plane, 5 constrain 5D (near-perfect)
- Use per-recording z-score normalization for device-agnostic preprocessing
- If using < 6 sensors, provide a channel mapping (see electronic-nose README)

## See also

- [Session-invariance proof](https://github.com/opensmell/session-invariance)
- [Data Commons contribution guide](https://github.com/opensmell/data-commons)
