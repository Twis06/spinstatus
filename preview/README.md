# spinstatus preview

Static browser emulator for the 480 x 320 ESP32 washer panel for demonstration purposes. Open `index.html` directly or deploy this folder to Vercel.

Live: https://preview-mauve-gamma.vercel.app

Desktop users get full demo controls. Phone users see only the fitted machine interface.

Idle color logic:

- Green: wash is done and ready for pickup.
- Red: laundry is still loaded after a 5-minute grace period.
- Fairness: a new user can remove laundry based on the longest overdue idle time.
