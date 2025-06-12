#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/packet-socket-helper.h"
#include "ns3/packet-socket-address.h"
#include "ns3/applications-module.h"

using namespace ns3;

static void MoveAp(Ptr<Node> apNode, Ptr<MobilityModel> apMobility) {
    for (int i = 1; i <= 44; ++i) {
        Simulator::Schedule(Seconds(i), &MobilityModel::SetPosition, apMobility, Vector(i * 5.0, 0, 0));
    }
}

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServerApplication", LOG_LEVEL_INFO);

    // Create nodes: two stations and one access point
    NodeContainer staNodes;
    staNodes.Create(2);
    NodeContainer apNode;
    apNode.Create(1);

    // Install mobility models for all nodes
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(staNodes);
    mobility.Install(apNode);

    // Get mobility model for AP to schedule movement
    Ptr<MobilityModel> apMobility = apNode.Get(0)->GetObject<MobilityModel>();
    MoveAp(apNode.Get(0), apMobility);

    // Configure wifi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    WifiMacHelper mac;
    Ssid ssid = Ssid("test-wifi-network");

    // Setup PHY
    YansWifiPhyHelper phy;
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    // Configure MAC for AP
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, apNode);

    // Configure MAC for STAs
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, staNodes);

    // Enable tracing for MAC and PHY events
    phy.EnablePcapAll("wifi-trace");

    // Install internet stack
    InternetStackHelper stack;
    stack.InstallAll();

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces;
    Ipv4InterfaceContainer apInterface;
    staInterfaces = address.Assign(staDevices);
    apInterface = address.Assign(apDevice);

    // Install packet socket capabilities
    PacketSocketHelper packetSocket;
    packetSocket.Install(staNodes);
    packetSocket.Install(apNode);

    // Set up UDP server on AP
    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(apNode.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(44.0));

    // Set up UDP client on both stations
    UdpClientHelper client(apInterface.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(500)); // bytes

    ApplicationContainer clientApp1 = client.Install(staNodes.Get(0));
    ApplicationContainer clientApp2 = client.Install(staNodes.Get(1));
    clientApp1.Start(Seconds(0.5));
    clientApp1.Stop(Seconds(44.0));
    clientApp2.Start(Seconds(0.5));
    clientApp2.Stop(Seconds(44.0));

    // Run simulation
    Simulator::Stop(Seconds(44.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}