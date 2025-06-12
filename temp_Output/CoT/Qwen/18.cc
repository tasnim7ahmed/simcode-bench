#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/packet-socket-client-server-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WirelessNetworkSimulation");

void MoveAp(Ptr<Node> apNode, Ptr<MobilityModel> mobility) {
    Vector position = mobility->GetPosition();
    position.x += 5.0;
    mobility->SetPosition(Vector(position.x, position.y, position.z));
    if (position.x < 100.0) {
        Simulator::Schedule(Seconds(1.0), &MoveAp, apNode, mobility);
    }
}

int main(int argc, char *argv[]) {
    uint32_t simulationTime = 44;
    std::string phyMode("DsssRate1Mbps");
    uint32_t packetSize = 1000;
    uint32_t numPackets = 1000000;
    double interval = 0.00142; // Adjusted for ~500kbps
    bool enableCtsRts = false;

    NodeContainer nodes;
    nodes.Create(3);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    YansWifiPhyHelper phy;
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("test-network");

    // AP node configuration
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, nodes.Get(0));

    // STA nodes configuration
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices;
    staDevices = mac.Install(phy, nodes.Get(1));
    staDevices.Add(mac.Install(phy, nodes.Get(2)));

    // Mobility setup
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(nodes);

    // Assign velocities and initial positions
    Ptr<MobilityModel> apMobility = nodes.Get(0)->GetObject<MobilityModel>();
    apMobility->SetPosition(Vector(0.0, 0.0, 0.0));
    nodes.Get(1)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(1.0, 0.0, 0.0));
    nodes.Get(2)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(-1.0, 0.0, 0.0));

    // Schedule movement of AP every second
    Simulator::Schedule(Seconds(1.0), &MoveAp, nodes.Get(0), apMobility);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    // Enable packet socket capabilities
    PacketSocketHelper packetSocket;
    packetSocket.Install(nodes);

    // Configure applications
    PacketSocketAddress socketAddress(InetSocketAddress(staInterfaces.GetAddress(0), 9));

    PacketSocketClientHelper client(staInterfaces.GetAddress(1), 9);
    client.SetAttribute("PacketSize", UintegerValue(packetSize));
    client.SetAttribute("NumPackets", UintegerValue(numPackets));
    client.SetAttribute("Interval", TimeValue(Seconds(interval)));
    ApplicationContainer clientApp = client.Install(nodes.Get(2));
    clientApp.Start(Seconds(0.5));
    clientApp.Stop(Seconds(simulationTime));

    PacketSocketServerHelper server;
    ApplicationContainer serverApp = server.Install(nodes.Get(1));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simulationTime));

    // Tracing
    phy.EnablePcapAll("wireless_network_trace");

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}