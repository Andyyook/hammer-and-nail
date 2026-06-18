/// Request ID propagation module.
///
/// Provides utilities for extracting, generating, and validating
/// request IDs (X-Request-Id) across the backend system.
///
/// The request ID is used to correlate logs and API responses
/// across distributed services.

use uuid::Uuid;

/// Maximum length for an incoming request ID header value.
pub const MAX_REQUEST_ID_LENGTH: usize = 128;

/// Extract or generate a request ID from an optional header value.
///
/// - If the header is present, non-empty, and ≤ 128 chars → use it
/// - If the header is missing, empty, or invalid → generate a UUID
pub fn extract_or_generate(header_value: Option<&str>) -> String {
    match header_value {
        Some(val) if is_valid(val) => val.to_string(),
        _ => generate(),
    }
}

/// Validate a request ID string.
///
/// Rules:
/// - Must be non-empty
/// - Must be ≤ 128 characters
pub fn is_valid(request_id: &str) -> bool {
    !request_id.is_empty() && request_id.len() <= MAX_REQUEST_ID_LENGTH
}

/// Generate a new UUID v4 request ID.
pub fn generate() -> String {
    Uuid::new_v4().to_string()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_generates_uuid() {
        let id = generate();
        assert!(!id.is_empty());
        assert_eq!(id.len(), 36); // UUID v4 format
    }

    #[test]
    fn test_valid_request_id() {
        assert!(is_valid("abc-123"));
        assert!(is_valid(&"a".repeat(128)));
    }

    #[test]
    fn test_invalid_request_id() {
        assert!(!is_valid(""));
        assert!(!is_valid(&"a".repeat(129)));
    }

    #[test]
    fn test_extract_or_generate_with_valid_header() {
        let result = extract_or_generate(Some("my-request-id"));
        assert_eq!(result, "my-request-id");
    }

    #[test]
    fn test_extract_or_generate_with_missing_header() {
        let result = extract_or_generate(None);
        // Should generate a UUID
        assert_eq!(result.len(), 36);
    }

    #[test]
    fn test_extract_or_generate_with_empty_header() {
        let result = extract_or_generate(Some(""));
        // Should generate a UUID since empty is invalid
        assert_eq!(result.len(), 36);
    }

    #[test]
    fn test_extract_or_generate_with_oversized_header() {
        let long = &"a".repeat(129);
        let result = extract_or_generate(Some(long));
        // Should generate a UUID since > 128 chars is invalid
        assert_eq!(result.len(), 36);
    }
}
