#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/packet-socket-client-server-helper.h"
#include "ns3/packet-socket-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiTracingSimulation");

void MoveAp(Ptr<Node> apNode) {
    Ptr<MobilityModel> mobility = apNode->GetObject<MobilityModel>();
    Vector position = mobility->GetPosition();
    position.x += 5.0;
    mobility->SetPosition(Vector(position.x, position.y, position.z));
    Simulator::Schedule(Seconds(1.0), &MoveAp, apNode);
}

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("OfdmRate54Mbps"), "ControlMode", StringValue("OfdmRate54Mbps"));

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(Ssid("ns3-wifi")),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(Ssid("ns3-wifi")));

    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
    mobility.Install(wifiStaNodes);

    PacketSocketHelper packetSocket;
    packetSocket.Install(wifiStaNodes);
    packetSocket.Install(wifiApNode);

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    PacketSocketAddress socketAddr(Ipv4Address::GetAny(), 80, 0);

    PacketSocketClientHelper client(socketAddr);
    client.SetAttribute("PacketSize", UintegerValue(1024));
    client.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    client.SetAttribute("Interval", TimeValue(Seconds(0.02)));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i) {
        clientApps.Add(client.Install(wifiStaNodes.Get(i)));
        clientApps.Get(i)->SetStartTime(Seconds(0.5));
        clientApps.Start(Seconds(0.5));
        clientApps.Stop(Seconds(44.0));
    }

    PacketSocketServerHelper server(socketAddr);
    ApplicationContainer serverApps = server.Install(wifiApNode);
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(44.0));

    AsciiTraceHelper asciiTraceHelper;
    phy.EnableAsciiAll(asciiTraceHelper.CreateFileStream("wifi-tracing-macPHY.txt"));

    phy.EnablePcapAll("wifi-tracing-pcap");

    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/State/RxOk", MakeCallback([](std::string path, Ptr<const Packet> packet, double snr, WifiTxVector txVector, std::pair<WifiCodeRate, uint8_t> codeRate, uint16_t staId) {
        NS_LOG_INFO("PHY RX OK at " << Simulator::Now().As(Time::S) << " from " << path << " size: " << packet->GetSize());
    }));

    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/State/RxError", MakeCallback([](std::string path, Ptr<const Packet> packet, double snr) {
        NS_LOG_INFO("PHY RX Error at " << Simulator::Now().As(Time::S) << " on " << path << " size: " << packet->GetSize());
    }));

    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MacRx", MakeCallback([](std::string path, Ptr<const Packet> packet) {
        NS_LOG_INFO("MAC RX at " << Simulator::Now().As(Time::S) << " on " << path << " size: " << packet->GetSize());
    }));

    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MacTx", MakeCallback([](std::string path, Ptr<const Packet> packet) {
        NS_LOG_INFO("MAC TX at " << Simulator::Now().As(Time::S) << " on " << path << " size: " << packet->GetSize());
    }));

    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/State/StateChange", MakeCallback([](std::string path, Time now, WifiPhyState state) {
        NS_LOG_INFO("PHY State Change to " << state << " at " << now.As(Time::S) << " on " << path);
    }));

    Simulator::Schedule(Seconds(1.0), &MoveAp, wifiApNode.Get(0));

    Simulator::Stop(Seconds(44.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}