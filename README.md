# HOTEL KINGPIN — C++ / Docker Hotel Booking Site

## Current build: restaurant menu, side drawer, registration and login
See `START_HERE.txt` for safe Windows/Docker update instructions.

## New: Guest registration and login
The first visit to `http://localhost:8093` opens the **Sign in** screen. Press **Create an account** to register using name, email address, Indian 10-digit phone number, and password (10+ characters). Later, sign in with either registered email **or** phone number plus the password. Click **Account → Sign out** to log out. Bookings and food orders are associated with the account, so they remain visible after login on a different device using the same database.

**Important:** Google OAuth / Sign in with Google, Gmail inbox verification, SMS OTP, password reset, and real identity ownership checks are **not implemented**. This is password-based authentication for local demonstration, not production-ready public authentication. Do not use real passwords. For public launch, add HTTPS (`Secure` cookies), CSRF protections, verified email/SMS, password-reset, account recovery, automated security tests and operational hardening.

Existing legacy anonymous guest bookings are not automatically transferred to new registered accounts. The previous SQLite database is preserved only if you keep the same Docker Compose volume.

## NEW: Full dining menu + swipe-open navigation
- The Dining page at `/menu` now lists all 26 sample menu items, grouped into Breakfast (7–11 AM), Lunch (12–3 PM), Snacks & Beverages (4–7 PM), and Dinner (7–10 PM). Including Idli, Poori, Veg meals, Bagara rice, Chicken curry, Chapathi, Phulka with Paneer/Chicken, snacks, and more.
- Dining prices are **illustrative editable values**, configured in `src/model.hpp`. There were only six items in the earlier supplied ZIP; this restores the requested categories using sample prices, not an external authoritative restaurant price list.
- Menu items are always visible without any booking; ordering fields only appear for a signed-in user with a paid booking. Chapathi and Plain phulka use per-piece pricing with a `Number of pieces` entry.
- The sticky header has a Menu button. On small touchscreens, a **swipe right from the left edge** opens a slide-out drawer. Swipe left, press ×, tap the backdrop, or Escape to close. Drawer links go to Home, Room Availability, Dining, My Stays and My Account.
- The C++ handler validates the dynamic dish index (no longer limited to six dishes). Quantity is limited to 1–10.

## Rooms and pricing
* Floor 1: 101–107, seven Non-AC single-bed rooms: ₹1,099 per night.
* Floor 2: 201–203, three Non-AC single-bed rooms: ₹1,099 per night; 204–206, three AC double-bed rooms: ₹2,499 per night.
* Floor 3: 301–307, seven AC single-bed rooms: ₹1,499 per night.
* Floor 4: 401–406, six AC double-bed rooms: ₹2,499 per night.
* Non-AC double tariff is ₹1,999 in the backend but **there are no currently assigned rooms of that type**.

## Windows 11, CMD and Docker steps
1. Extract ZIP into a new folder.
2. Open folder containing `compose.yaml`; type `cmd` in File Explorer address bar.
3. To avoid conflict with the old app on port 8092, run:

```cmd
set "HOTEL_PORT=8093"
docker compose up -d --build
docker compose ps
```

4. Visit `http://localhost:8093`. First time choose **Create account**, then log in; search availability and book.
5. To stop: `docker compose down` (never `docker compose down -v` if you want the database).

**Data note:** By default this creates a new Compose project/volume if your old project name differs. To preserve existing bookings, don't delete old volumes; changing project names or compose file location can result in using a different volume.

## Tech
C++20 cpp-httplib HTTP server, embedded SQLite, PBKDF2-HMAC-SHA256 salted password hashes via OpenSSL, SQLite-backed random session tokens, simple login attempt throttling, `HttpOnly; SameSite=Strict` cookies, server-rendered HTML/CSS. All images are included locally. Existing demo payment is **simulation** only, not real money.
