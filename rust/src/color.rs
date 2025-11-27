use crate::TypeValueEncodable;

#[cfg(feature = "color-name")]
pub use color_name::css::colors::*;

/// Represents a color in use for controlling appearance within NSight profilers.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Color {
    /// alpha channel
    a: u8,
    /// red channel
    r: u8,
    /// green channel
    g: u8,
    /// blue channel
    b: u8,
}

impl From<[u8; 3]> for Color {
    fn from(value: [u8; 3]) -> Self {
        Color {
            a: 255,
            r: value[0],
            g: value[1],
            b: value[2],
        }
    }
}

/// Convert from u32 (hex) to Color
///
/// If the most-significant (BE) byte is 00, then treat the value as RGB
/// Else the most-significant (BE) byte represents the alpha channel (ARGB)
impl From<u32> for Color {
    fn from(value: u32) -> Self {
        let [a, r, g, b] = value.to_be_bytes();
        if value & 0xFFFFFF == value {
            Color { a: 255, r, g, b }
        } else {
            Color { a, r, g, b }
        }
    }
}

impl Color {
    /// Create a new color from specified channels
    ///
    /// ```
    /// let nice_blue = nvtx::Color::new(0, 192, 255, 255);
    /// ```
    pub fn new(r: u8, g: u8, b: u8, a: u8) -> Self {
        Self { a, r, g, b }
    }

    /// Change the alpha channel for the color and yield a new color
    ///
    /// ```
    /// let nice_blue = nvtx::Color::new(0, 192, 255, 255);
    /// let translucent_blue = nice_blue.with_alpha(128);
    /// ```
    pub fn with_alpha(&self, a: u8) -> Self {
        Color { a, ..*self }
    }

    /// Change the red channel for the color and yield a new color
    ///
    /// ```
    /// let dark_gray = nvtx::Color::new(32, 32, 32, 255);
    /// let dark_red = dark_gray.with_red(128);
    /// ```
    pub fn with_red(&self, r: u8) -> Self {
        Color { r, ..*self }
    }

    /// Change the green channel for the color and yield a new color
    ///
    /// ```
    /// let dark_gray = nvtx::Color::new(32, 32, 32, 255);
    /// let dark_green = dark_gray.with_green(128);
    /// ```
    pub fn with_green(&self, g: u8) -> Self {
        Color { g, ..*self }
    }

    /// Change the blue channel for the color and yield a new color
    ///
    /// ```
    /// let dark_gray = nvtx::Color::new(32, 32, 32, 255);
    /// let dark_blue = dark_gray.with_blue(128);
    /// ```
    pub fn with_blue(&self, b: u8) -> Self {
        Color { b, ..*self }
    }

    /// Get the value of the alpha channel
    pub fn alpha(&self) -> u8 {
        self.a
    }

    /// Get the value of the red channel
    pub fn red(&self) -> u8 {
        self.r
    }

    /// Get the value of the green channel
    pub fn green(&self) -> u8 {
        self.g
    }

    /// Get the value of the blue channel
    pub fn blue(&self) -> u8 {
        self.b
    }
}

impl TypeValueEncodable for Color {
    type Type = nvtx_sys::ColorType;
    type Value = u32;

    fn encode(&self) -> (Self::Type, Self::Value) {
        let as_u32 = u32::from_be_bytes([self.a, self.r, self.g, self.b]);
        (Self::Type::NVTX_COLOR_ARGB, as_u32)
    }

    fn default_encoding() -> (Self::Type, Self::Value) {
        (Self::Type::NVTX_COLOR_UNKNOWN, 0)
    }
}

#[cfg(test)]
mod tests {
    use crate::{common::TestUtils, TypeValueEncodable};

    use super::Color;

    #[test]
    fn ctor() {
        let color = Color::new(0xFF, 0xEE, 0xDD, 0xCC);
        assert_eq!(color.a, 0xCC);
        assert_eq!(color.r, 0xFF);
        assert_eq!(color.g, 0xEE);
        assert_eq!(color.b, 0xDD);
    }

    #[test]
    fn accessors() {
        let color = Color::new(0x12, 0x34, 0x56, 0x78);
        assert_eq!(color.r, color.red());
        assert_eq!(color.g, color.green());
        assert_eq!(color.b, color.blue());
        assert_eq!(color.a, color.alpha());
    }

    #[test]
    fn from_u8_array() {
        let arr: [u8; 3] = [0xAB, 0xCD, 0xEF];
        let color = Color::from(arr);
        assert_eq!(color.a, 0xFF);
        assert_eq!(color.r, 0xAB);
        assert_eq!(color.g, 0xCD);
        assert_eq!(color.b, 0xEF);
    }

    #[test]
    fn from_u32_mask_0xffffff() {
        let color = Color::from(0xABCDEF);
        assert_eq!(color.a, 0xFF);
        assert_eq!(color.r, 0xAB);
        assert_eq!(color.g, 0xCD);
        assert_eq!(color.b, 0xEF);
    }

    #[test]
    fn from_u32_mask_0xffffffff() {
        let color = Color::from(0xFEDCBA98);
        assert_eq!(color.a, 0xFE);
        assert_eq!(color.r, 0xDC);
        assert_eq!(color.g, 0xBA);
        assert_eq!(color.b, 0x98);
    }

    #[test]
    fn with() {
        let black = Color::new(0, 0, 0, 0);
        let red = Color::new(0xFF, 0, 0, 0);
        let green = Color::new(0, 0xFF, 0, 0);
        let blue = Color::new(0, 0, 0xFF, 0);
        let transparent = Color::new(0, 0, 0, 0xFF);
        assert_eq!(black.with_red(0xFF), red);
        assert_eq!(black.with_green(0xFF), green);
        assert_eq!(black.with_blue(0xFF), blue);
        assert_eq!(black.with_alpha(0xFF), transparent);
    }

    #[test]
    fn encode() {
        let (r, g, b, a) = (0x12, 0x34, 0x56, 0x78);
        let color = Color::new(r, g, b, a);
        TestUtils::assert_color_encoding(&color, r, g, b, a);
    }

    #[test]
    fn test_encode_default() {
        let (t, v) = Color::default_encoding();
        assert_eq!(t, nvtx_sys::ColorType::NVTX_COLOR_UNKNOWN);
        assert_eq!(v, 0);
    }
}
