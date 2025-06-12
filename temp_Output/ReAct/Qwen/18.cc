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
    Simulator::Schedule(Seconds(1.0), &MoveAp, apNode, mobility);
}

int main(int argc, char *argv[]) {
    uint32_t simulationTime = 44;
    std::string phyMode("DsssRate1Mbps");
    double dataRateKbps = 500;
    std::string ssidStr = "wifi-network";

    Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue(phyMode));

    NodeContainer nodes;
    nodes.Create(3);

    Ptr<Node> apNode = nodes.Get(0);
    Ptr<Node> sta1Node = nodes.Get(1);
    Ptr<Node> sta2Node = nodes.Get(2);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue(phyMode), "ControlMode", StringValue(phyMode));

    WifiMacHelper wifiMac;
    Ssid ssid = Ssid(ssidStr);

    wifiMac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid),
                    "BeaconGeneration", BooleanValue(true),
                    "BeaconInterval", TimeValue(Seconds(2.5)));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(wifiPhy, wifiMac, apNode);

    wifiMac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid),
                    "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices;
    staDevices = wifi.Install(wifiPhy, wifiMac, nodes.Get(1));
    staDevices.Add(wifi.Install(wifiPhy, wifiMac, nodes.Get(2)));

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

    for (NodeContainer::Iterator it = nodes.Begin(); it != nodes.End(); ++it) {
        Ptr<MobilityModel> mob = (*it)->GetObject<MobilityModel>();
        mob->SetVelocity(Vector(0.0, 0.0, 0.0));
    }

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    PacketSocketHelper packetSocket;
    packetSocket.Install(nodes);

    PacketSocketAddress socketAddress;
    socketAddress.SetSingleDevice(staInterfaces.GetAddress(0));
    socketAddress.SetPhysicalAddress(staInterfaces.GetAddress(1));
    socketAddress.SetProtocol(0);

    PacketSocketClientHelper client(sta1Node->GetId());
    client.SetAttribute("PacketSize", UintegerValue(1024));
    client.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    client.SetAttribute("Interval", TimeValue(Seconds(1024.0 / (dataRateKbps / 8))));

    ApplicationContainer clientApp = client.Install(sta1Node);
    clientApp.Start(Seconds(0.5));
    clientApp.Stop(Seconds(simulationTime));

    PacketSocketServerHelper server;
    ApplicationContainer serverApp = server.Install(sta2Node);
    serverApp.Start(Seconds(0.5));
    serverApp.Stop(Seconds(simulationTime));

    AsciiTraceHelper asciiTraceHelper;
    wifiPhy.EnableAsciiAll(asciiTraceHelper.CreateFileStream("wireless-mac.tr"));

    wifiPhy.EnablePcapAll("wireless-phy", false);

    Simulator::Schedule(Seconds(1.0), &MoveAp, apNode, apNode->GetObject<MobilityModel>());

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}