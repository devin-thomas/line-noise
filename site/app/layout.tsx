import type { Metadata, Viewport } from "next";
import "./globals.css";

export const metadata: Metadata = {
  title: "LINE NOISE! | Playable GBA Vertical Slice",
  description:
    "Patch calls, shape crosstalk, and decide what Port Meridian remembers in this playable Game Boy Advance vertical slice from Uppercut Labs.",
  icons: {
    icon: "/brand/uppercut-labs-logo.png",
    shortcut: "/brand/uppercut-labs-logo.png",
  },
  openGraph: {
    title: "LINE NOISE!",
    description: "Connections carry people. Crossings carry information.",
    type: "website",
    images: [
      {
        url: "/screens/four-live-cords.png",
        width: 240,
        height: 160,
        alt: "Four live, crossing cords on the LINE NOISE patchboard.",
      },
    ],
  },
};

export const viewport: Viewport = {
  themeColor: "#07151d",
  colorScheme: "dark",
};

export default function RootLayout({
  children,
}: Readonly<{
  children: React.ReactNode;
}>) {
  return (
    <html lang="en">
      <body>{children}</body>
    </html>
  );
}
