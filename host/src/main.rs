// See the "macOS permissions note" in README.md before running this on macOS
// Big Sur or later.

mod rpc_ble;

use btleplug::api::{
    bleuuid::uuid_from_u16, Central, Manager as _, Peripheral as _, ScanFilter, WriteType,
};
use btleplug::platform::{Adapter, Manager, Peripheral};
use std::error::Error;
use std::time::Duration;
use uuid::Uuid;

const LIGHT_CHARACTERISTIC_UUID: Uuid = uuid_from_u16(0xFF52);
use tokio::time;

async fn find_light(central: &Adapter) -> Option<Peripheral> {
    for p in central.peripherals().await.unwrap() {
        if p.properties()
            .await
            .unwrap()
            .unwrap()
            .local_name
            .iter()
            .any(|name| name.contains("ble"))
        {
            return Some(p);
        }
    }
    None
}

#[tokio::main]
async fn main() -> Result<(), Box<dyn Error>> {
    pretty_env_logger::init();

    let manager = Manager::new().await.unwrap();

    // get the first bluetooth adapter
    let central = manager
        .adapters()
        .await
        .expect("Unable to fetch adapter list.")
        .into_iter()
        .nth(0)
        .expect("Unable to find adapters.");

    // start scanning for devices
    central.start_scan(ScanFilter::default()).await?;
    // instead of waiting, you can use central.events() to get a stream which will
    // notify you of new devices, for an example of that see examples/event_driven_discovery.rs
    time::sleep(Duration::from_secs(2)).await;

    // find the device we're interested in
    let light = find_light(&central).await.expect("No lights found");

    // connect to the device
    light.connect().await?;

    // discover services and characteristics
    light.discover_services().await?;

    // find the characteristic we want
    let chars = light.characteristics();
    chars.iter().for_each(|c| println!("uuid: {:?}", c.uuid));

    let cmd_char = chars
        .iter()
        .find(|c| c.uuid == LIGHT_CHARACTERISTIC_UUID)
        .expect("Unable to find characterics");

    // for cmd_char in chars.iter() {
    println!("testing uuid: {:?}", cmd_char.uuid);
    // // dance party
    // // let mut rng = thread_rng();
    // for _ in 0..(32*(1024/256)) {
    //     // let color_cmd = vec![rng.gen()];
    //     // let color_cmd = vec![0x34, 0x12];
    //     let color_cmd: Vec<u8> = Vec::from_iter(std::iter::repeat(4 as u8).take(251));
    //     light
    //         .write(&cmd_char, &color_cmd,
    //                // WriteType::WithResponse)
    //                WriteType::WithoutResponse)
    //         .await?;
    //     // let ret = light.read(&cmd_char).await?;
    //     // if ret.len() < 32 {
    //     //     println!("read: {:?}", ret);
    //     // }
    //     // time::sleep(Duration::from_millis(20)).await;
    // }
    let frame = rpc_ble::BleFrame::new_command("rpc_ping", &[]);
    let data: Vec<u8> = frame.into();
    light
        .write(&cmd_char, &data,
               // WriteType::WithResponse)
               WriteType::WithoutResponse)
        .await?;
    println!("tested uuid: {:?}", cmd_char.uuid);
    time::sleep(Duration::from_millis(2000)).await;
    // }
    Ok(())
}
