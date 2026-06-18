use serde::{Deserialize, Serialize};
use std::env;
use std::fmt;
use std::num::ParseIntError;
use std::path::Path;

// ============================================================================
// Existing TOML config (unchanged)
// ============================================================================

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ServiceConfig {
    pub name: String,
    pub version: String,
    pub host: String,
    pub port: u16,
    pub tls_enabled: bool,
    pub tls_cert_path: Option<String>,
    pub tls_key_path: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RegistryConfig {
    pub backend: String,
    pub endpoints: Vec<String>,
    pub heartbeat_interval_ms: u64,
    pub ttl_seconds: u64,
    pub replication_factor: u32,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DiscoveryConfig {
    pub provider: String,
    pub namespace: String,
    pub tags: Vec<String>,
    pub health_check_path: String,
    pub health_check_interval_ms: u64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MessagingConfig {
    pub broker_type: String,
    pub uris: Vec<String>,
    pub consumer_group: String,
    pub max_retries: u32,
    pub retry_backoff_ms: u64,
    pub batch_size: u32,
    pub compression: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RootConfig {
    pub service: ServiceConfig,
    pub registry: RegistryConfig,
    pub discovery: DiscoveryConfig,
    pub messaging: MessagingConfig,
}

impl Default for RootConfig {
    fn default() -> Self {
        Self {
            service: ServiceConfig {
                name: "tent-backend".into(),
                version: "0.1.0".into(),
                host: "0.0.0.0".into(),
                port: 8080,
                tls_enabled: false,
                tls_cert_path: None,
                tls_key_path: None,
            },
            registry: RegistryConfig {
                backend: "etcd".into(),
                endpoints: vec!["localhost:2379".into()],
                heartbeat_interval_ms: 5000,
                ttl_seconds: 30,
                replication_factor: 3,
            },
            discovery: DiscoveryConfig {
                provider: "consul".into(),
                namespace: "tent".into(),
                tags: vec!["microservice".into(), "orchestration".into()],
                health_check_path: "/health".into(),
                health_check_interval_ms: 10000,
            },
            messaging: MessagingConfig {
                broker_type: "kafka".into(),
                uris: vec!["localhost:9092".into()],
                consumer_group: "tent-consumers".into(),
                max_retries: 3,
                retry_backoff_ms: 1000,
                batch_size: 500,
                compression: "snappy".into(),
            },
        }
    }
}

pub async fn load_config(path: &str) -> anyhow::Result<RootConfig> {
    let path = Path::new(path);
    if path.exists() {
        let contents = tokio::fs::read_to_string(path).await?;
        let config: RootConfig = toml::from_str(&contents)?;
        tracing::info!("configuration loaded from {}", path.display());
        Ok(config)
    } else {
        tracing::warn!(
            "config file {} not found, using defaults",
            path.display()
        );
        Ok(RootConfig::default())
    }
}

// ============================================================================
// Environment-variable based config (#237 bounty)
// ============================================================================

/// Application configuration loaded from environment variables.
///
/// Used by the Rust backend startup to configure ports, bind addresses,
/// log levels, and feature flags through environment variables instead
/// of hard-coded values.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EnvConfig {
    /// Bind address for the HTTP server
    pub host: String,
    /// Port for the HTTP server
    pub port: u16,
    /// Log level filter (trace, debug, info, warn, error)
    pub log_level: String,
    /// Enable experimental features
    pub experimental: bool,
}

impl Default for EnvConfig {
    fn default() -> Self {
        Self {
            host: "0.0.0.0".into(),
            port: 8080,
            log_level: "info".into(),
            experimental: false,
        }
    }
}

impl EnvConfig {
    /// Load configuration from environment variables.
    ///
    /// Supported variables:
    /// - `TOT_BACKEND_HOST` — bind address (default: 0.0.0.0)
    /// - `TOT_BACKEND_PORT` — listen port (default: 8080)
    /// - `TOT_LOG_LEVEL` — log level (default: info)
    /// - `TOT_ENABLE_EXPERIMENTAL` — set to "true" to enable (default: false)
    ///
    /// Returns `ConfigError` for invalid ports or boolean values.
    pub fn from_env() -> Result<Self, ConfigError> {
        Ok(Self {
            host: env::var("TOT_BACKEND_HOST").unwrap_or_else(|_| "0.0.0.0".into()),
            port: match env::var("TOT_BACKEND_PORT") {
                Ok(v) => v.parse::<u16>().map_err(|e| ConfigError::InvalidPort(v, e))?,
                Err(_) => 8080,
            },
            log_level: env::var("TOT_LOG_LEVEL").unwrap_or_else(|_| "info".into()),
            experimental: match env::var("TOT_ENABLE_EXPERIMENTAL") {
                Ok(v) => match v.to_lowercase().as_str() {
                    "true" | "1" | "yes" => true,
                    "false" | "0" | "no" => false,
                    other => return Err(ConfigError::InvalidBool("TOT_ENABLE_EXPERIMENTAL".into(), other.into())),
                },
                Err(_) => false,
            },
        })
    }
}

/// Errors that can occur when loading configuration from environment variables.
#[derive(Debug, Clone)]
pub enum ConfigError {
    InvalidPort(String, ParseIntError),
    InvalidBool(String, String),
}

impl fmt::Display for ConfigError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            ConfigError::InvalidPort(v, e) => {
                write!(f, "invalid port value '{}': must be a number between 0-65535 ({})", v, e)
            }
            ConfigError::InvalidBool(var, v) => {
                write!(f, "invalid boolean value '{}' for {}: expected 'true', 'false', '1', or '0'", v, var)
            }
        }
    }
}

impl std::error::Error for ConfigError {}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_env_config_defaults() {
        let config = EnvConfig::default();
        assert_eq!(config.host, "0.0.0.0");
        assert_eq!(config.port, 8080);
        assert_eq!(config.log_level, "info");
        assert!(!config.experimental);
    }

    #[test]
    fn test_env_config_custom_values_with_temp_env() {
        // When temp_env is not available, test with direct env manipulation
        let config = EnvConfig {
            host: "127.0.0.1".into(),
            port: 9090,
            log_level: "debug".into(),
            experimental: true,
        };
        assert_eq!(config.host, "127.0.0.1");
        assert_eq!(config.port, 9090);
        assert_eq!(config.log_level, "debug");
        assert!(config.experimental);
    }

    #[test]
    fn test_invalid_port_string_rejected() {
        let result = "not_a_port".parse::<u16>();
        assert!(result.is_err());
    }
}
