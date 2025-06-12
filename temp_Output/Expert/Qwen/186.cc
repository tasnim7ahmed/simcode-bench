#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/wave-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VanetSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create nodes for 5 vehicles
    NodeContainer nodes;
    nodes.Create(5);

    // Setup mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(20.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(5),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(nodes);

    // Start mobility
    for (auto node = nodes.Begin(); node != nodes.End(); ++node) {
        Ptr<MobilityModel> mobility = (*node)->GetObject<MobilityModel>();
        Ptr<ConstantVelocityMobilityModel> cvmm = mobility->GetObject<ConstantVelocityMobilityModel>();
        if (cvmm) {
            cvmm->SetVelocity(Vector(10.0, 0.0, 0.0)); // Move along X-axis at 10 m/s
        }
    }

    // Configure WAVE PHY and MAC
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    // Set 802.11p standard
    WifiMacHelper mac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211p);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("OfdmRate6MbpsBW10MHz"),
                                 "ControlMode", StringValue("OfdmRate6MbpsBW10MHz"));

    // Install WAVE devices
    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // Install internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Use OnOffApplication to send BSMs periodically
    uint16_t port = 8080;
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("PacketSize", UintegerValue(200));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("1kbps")));

    ApplicationContainer apps;
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        // Send BSM to all other nodes
        for (uint32_t j = 0; j < nodes.GetN(); ++j) {
            if (i != j) {
                onoff.SetAttribute("Remote", AddressValue(InetSocketAddress(interfaces.GetAddress(j), port)));
                apps = onoff.Install(nodes.Get(i));
                apps.Start(Seconds(1.0));
                apps.Stop(Seconds(10.0));
            }
        }
    }

    // Enable NetAnim visualization
    AnimationInterface anim("vanet_simulation.xml");
    anim.SetMobilityPollInterval(Seconds(0.5));

    // Schedule simulation stop
    Simulator::Stop(Seconds(10.0));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}