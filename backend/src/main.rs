use anyhow::Result;
use axum::{
    Router,
    response::IntoResponse,
    routing::get,
};
use clap::Parser;
use std::net::SocketAddr;
use tent_backend::config::EnvConfig;
use tent_backend::discovery::ServiceDiscovery;
use tent_backend::messaging::MessageBroker;
use tent_backend::registry::ServiceRegistry;
// request_id module available: use tent_backend::request_id;
use tracing_subscriber::EnvFilter;

#[derive(Parser, Debug)]
#[command(name = "tent-backend")]
#[command(about = "Tent of Trials Backend - Distributed Microservices Framework", long_about = None)]
struct Cli {
    #[arg(short, long, default_value = "node-0")]
    node_id: String,

    #[arg(short, long)]
    consensus: bool,

    #[arg(long, default_value_t = 10000)]
    max_connections: u32,

    #[arg(short, long, default_value = "/etc/tent/config.toml")]
    config: String,
}

async fn health() -> impl IntoResponse {
    axum::Json(serde_json::json!({"status": "ok"}))
}

#[tokio::main]
async fn main() -> Result<()> {
    tracing_subscriber::fmt()
        .with_env_filter(EnvFilter::try_from_default_env().unwrap_or_else(|_| "info".into()))
        .json()
        .init();

    let cli = Cli::parse();
    let env_config = EnvConfig::from_env().unwrap_or_default();
    tracing::info!(log_level = %env_config.log_level, experimental = %env_config.experimental, "env configuration loaded");

    tracing::info!(
        node_id = %cli.node_id,
        consensus = %cli.consensus,
        max_connections = %cli.max_connections,
        config = %cli.config,
        "initializing tent backend orchestration framework"
    );

    let config = tent_backend::config::load_config(&cli.config).await?;
    let registry = ServiceRegistry::new(config.registry.clone());
    let discovery = ServiceDiscovery::new(config.discovery.clone());
    let broker = MessageBroker::new(config.messaging.clone());

    registry.initialize().await?;
    discovery.announce(&cli.node_id).await?;
    broker.connect().await?;

    let app = Router::new().route("/health", get(health));

    let addr = SocketAddr::new(
        env_config.host.parse().unwrap_or(std::net::IpAddr::V4(std::net::Ipv4Addr::new(0, 0, 0, 0))),
        env_config.port,
    );

    tracing::info!("health server listening on {}", addr);

    tokio::select! {
        _ = axum::serve(
            tokio::net::TcpListener::bind(addr).await?,
            app,
        ) => {}
        _ = {
            let mut signal = tokio::signal::unix::signal(
                tokio::signal::unix::SignalKind::terminate(),
            )?;
            async move { signal.recv().await; }
        } => {}
        _ = tokio::signal::ctrl_c() => {
            tracing::info!("received SIGINT, initiating graceful shutdown");
        }
    }

    broker.disconnect().await?;
    discovery.withdraw(&cli.node_id).await?;
    registry.shutdown().await?;

    tracing::info!("shutdown complete");
    Ok(())
}