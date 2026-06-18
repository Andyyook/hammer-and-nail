/// Build script for tent-backend.
///
/// Verifies that all protocol types with Serialize/Deserialize derive
/// have an explicit rename_all attribute to ensure consistent field naming.
use std::fs;
use std::path::Path;

fn main() {
    let proto_dir = Path::new("src/protocol");
    if !proto_dir.exists() { return; }

    let mut has_error = false;

    for entry in fs::read_dir(proto_dir).unwrap() {
        let entry = entry.unwrap();
        let path = entry.path();
        if !path.to_string_lossy().ends_with(".rs") { continue; }

        let content = fs::read_to_string(&path).unwrap();
        let lines: Vec<&str> = content.lines().collect();

        for (i, line) in lines.iter().enumerate() {
            if line.trim().starts_with("#[derive(")
                && line.contains("Serialize")
                && line.contains("Deserialize")
            {
                let mut found_rename = false;
                for j in (i + 1)..lines.len().min(i + 5) {
                    let next = lines[j].trim();
                    if next.starts_with("#[serde(") && next.contains("rename_all") {
                        found_rename = true;
                        break;
                    }
                    if !next.is_empty() && !next.starts_with("//") && !next.starts_with("#[") {
                        break;
                    }
                }
                if !found_rename {
                    println!("cargo:warning={}:{}: missing rename_all on Serialize/Deserialize type", path.display(), i + 1);
                    has_error = true;
                }
            }
        }
    }

    if has_error {
        panic!("Protocol validation: some Serde types lack #[serde(rename_all)]");
    }
}
