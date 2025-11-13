# Getting started
=================

## Two modes of keying in MACsec
1. Static keying (pure ip macsec … add key … commands)
You configure the Cipher Key (CAK) and Security Association (SA) manually.
Everything (TX key, RX key, association lifetimes, sequence numbers) stays fixed.
If you want to rotate the key, you must reconfigure by hand.
No negotiation or rekeying → truly static.

2. MKA (MACsec Key Agreement) with PSK (CAK/CKN)
You still provision a shared secret CAK (and its identifier CKN).
But that CAK is only used as the root key.
wpa_supplicant’s MKA protocol takes that CAK and dynamically derives Session Keys (SAKs).
MKA manages:
Which SA is active,
Rekeying on timer or when a new participant joins/leaves,
Automatic rollover of packet numbers and new SAs,
Key distribution to all peers in the Connectivity Association.
You see this on your system:
TXSC: … PN 7, state on, key d7fa521be9adcb54…
RXSC: … PN 2, state on, key d7fa521be9adcb54…
That d7fa521b… isn’t your CAK — it’s a dynamically derived SAK created by MKA.

Step 1: Recall what you provisioned
In your /etc/wpa_supplicant-macsec.conf you set:

mka_ckn=00112233445566778899aabbccddeeff
mka_cak=0123456789abcdef0123456789abcdef
That CAK (Connectivity Association Key) lives only inside the MKA engine in wpa_supplicant.
You will never see it directly in ip macsec show.

Step 2: Look at the MACsec interface state

ip macsec show

Typical output (yours looked similar) will be:

3: macsec0: protect on validate strict sc off sa off encrypt on ...
    cipher suite: GCM-AES-128, using ICV length 16
    TXSC: 080027adc3bd0001 on SA 0
        0: PN 7, state on, key d7fa521be9adcb541e06e13201000000
    RXSC: 080027d9d0310001, state on
        0: PN 2, state on, key d7fa521be9adcb541e06e13201000000

🔑 How to interpret this
TXSC/RXSC = Secure Channels (your transmit and your peer’s receive).
SA 0 = Security Association index (multiple can exist).
PN 7 / PN 2 = Packet Numbers (increasing counters).
Key d7fa521be9adcb54… = Session Key (SAK) currently used.
That long hex key is not your CAK. It is dynamically derived from the CAK by MKA using a key derivation function.
When rekeying happens, a new SAK will appear here while the old one ages out.

🔎 Step 3: Observe dynamic behavior
You can force rekeying or just wait. To observe

watch -n 2 "ip macsec show"
You’ll see:

The PN counter climbing (proof that packets are being encrypted).
After a rekey event, the “key …” string will change — a new SAK.
By default wpa_supplicant (MKA) periodically rekeys (the interval can be configured).

💡 Analogy
Think Wi‑Fi WPA2:

Your PSK (Wi‑Fi password) = CAK → Static secret.
Per‑session keys (PTKs/GTKs) = SAKs → Negotiated dynamically, rekeyed over time.
MACsec PSK works the same way.

✅ So in practice:

CKN/CAK = static config you must distribute out‑of‑band.
SAKs = dynamic keys shown under ip macsec show that change over time and do the actual data encryption.

🔧 How MKA Rekeying Works
MKA uses the Connectivity Association Key (CAK) you provisioned as input.
It derives Session Keys (SAKs) for the actual encryption.
It periodically refreshes SAKs to ensure forward secrecy and to avoid PN wraparound.
The rekey interval is controlled by the rekey timer inside wpa_supplicant.

1️⃣ Change the rekey interval
In your wpa_supplicant config network={} block, you can add:

mka_life_time=30

Value is seconds.
It tells MKA when to initiate a new SAK.
The default is usually 600s (10 minutes).
Setting it to 30 means you’ll see a new key negotiated about every half‑minute.

MACsec
RouteLLM
Routing to GPT-5
Awesome 🚀 — let’s make the SAK (Session Key) rekeying visible so you can see dynamic keying in action. By default, wpa_supplicant’s MKA will rekey, but the interval can be long. You can tune it.

🔧 How MKA Rekeying Works
MKA uses the Connectivity Association Key (CAK) you provisioned as input.
It derives Session Keys (SAKs) for the actual encryption.
It periodically refreshes SAKs to ensure forward secrecy and to avoid PN wraparound.
The rekey interval is controlled by the rekey timer inside wpa_supplicant.

1️⃣ Change the rekey interval
In your wpa_supplicant config network={} block, you can add:

ini
Copy
    mka_life_time=30
Value is seconds.
It tells MKA when to initiate a new SAK.
The default is usually 600s (10 minutes).
Setting it to 30 means you’ll see a new key negotiated about every half‑minute.

2️⃣ Example config with rekey interval

network={
    key_mgmt=NONE
    macsec_policy=1
    macsec_integ_only=0
    macsec_replay_protect=1
    macsec_replay_window=0
    macsec_port=1
    mka_ckn=00112233445566778899aabbccddeeff
    mka_cak=0123456789abcdef0123456789abcdef
    mka_life_time=30
}

3️⃣ How to observe

watch -n 5 "ip macsec show"

Expected:

PN keeps climbing with traffic.
Every ~30 seconds, you’ll see the “key …” hex value change in both TXSA and RXSA.
That’s MKA installing a new SAK.

4️⃣ Optional logging
You can also ask wpa_supplicant to log more detail (helpful for debugging):

sudo /usr/local/sbin/wpa_supplicant -i enp0s3 -D macsec_linux -c /etc/wpa_supplicant-macsec.conf -dd

Look for lines like MKA: New SAK generated with rekey events.

## 🚦  So, what’s “dynamic” here?
Static CAK/CKN → yes, they are provisioned and do not change unless you update your config.
Dynamic SAKs (per-session keys) → negotiated and rekeyed automatically by MKA using that static CAK as input.
This gives you:
Rekeying without manual intervention.
Multiple participants able to join/leave a secure group.
Automatic rollover before sequence numbers overflow.
Think of the CAK/CKN like a Wi‑Fi WPA2-PSK: you set a Wi‑Fi password (“static”), but the actual encryption keys (PTKs/GTKs) are dynamically derived and rotated by the protocol during 4‑way handshake. Same idea.

## ✅ Summary
Your CAK/CKN values = static root secret.
wpa_supplicant’s MKA = dynamic session key manager that derives & rotates actual traffic encryption keys (SAKs).
That’s why this mode is officially called “MACsec PSK (Pre‑Shared Key)” dynamic keying.