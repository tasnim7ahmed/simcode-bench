#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/lora-helper.h"
#include "ns3/lorawan-mac-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;
using namespace lorawan;

int main(int argc, char *argv[]) {
    // Enable logging for relevant components
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("LoraNetDevice", LOG_LEVEL_ALL);
    LogComponentEnable("LorawanMac", LOG_LEVEL_ALL);

    // Create nodes: 0 is the sink, 1 is the sensor
    NodeContainer nodes;
    nodes.Create(2);

    // Create LoRa channel
    Ptr<LoRaChannel> channel = CreateObject<LoRaChannel>();

    // Install LoRaNetDevices
    LoraHelper loraHelper;
    NetDeviceContainer devices = loraHelper.Install(nodes, channel);

    // Mobility setup (static)
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(100.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Install Internet stack with LoRaWAN MAC helper
    InternetStackHelper internet;
    LorawanMacHelper macHelper;
    macHelper.SetAddressGenerator(CreateObject<ClassCAddressGenerator>());
    internet.SetMacHelper(macHelper);
    internet.Install(nodes);

    // Assign global addresses (no IP address assignment needed for LoRaWAN in this context)

    // Set up UDP server (sink node) on node 0
    uint16_t port = 9;
    UdpEchoServerHelper server(port);
    ApplicationContainer serverApp = server.Install(nodes.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(20.0));

    // Set up UDP client (sensor node) on node 1
    UdpEchoClientHelper client(Ipv4Address::GetZero(), port);
    client.SetAttribute("MaxPackets", UintegerValue(10));
    client.SetAttribute("Interval", TimeValue(Seconds(2.0)));
    client.SetAttribute("PacketSize", UintegerValue(50));

    ApplicationContainer clientApp = client.Install(nodes.Get(1));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(20.0));

    // Animation interface
    AnimationInterface anim("lora-udp-simulation.xml");

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}