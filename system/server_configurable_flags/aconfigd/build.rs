use protobuf_codegen::Codegen;

fn main() {
    let proto_files = vec!["aconfigd.proto"];

    // tell cargo to only re-run the build script if any of the proto files has changed
    for path in &proto_files {
        println!("cargo:rerun-if-changed={}", path);
    }

    Codegen::new()
        .pure()
        .include(".")
        .inputs(proto_files)
        .cargo_out_dir("aconfigd_proto")
        .run_from_script();
}
