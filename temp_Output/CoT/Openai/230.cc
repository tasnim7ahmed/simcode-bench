#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiUdpClientsToServer");

void RxPacketLogger(Ptr<const Packet> packet, const Address &address)
{
    NS_LOG_UNCOND("Server received packet of size: " << packet->GetSize() << " bytes at " << Simulator::Now().GetSeconds() << "s from " << InetSocketAddress::ConvertFrom(address).GetIpv4());
}

int main(int argc, char *argv[])
{
    uint32_t nClients = 4;
    double simulationTime = 10.0;
    uint32_t packetSize = 1024;
    double interval = 1.0;
    uint16_t port = 4000;

    CommandLine cmd;
    cmd.AddValue("nClients", "Number of client nodes", nClients);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.Parse(argc, argv);

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(nClients);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi = WifiHelper::Default();
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi-sim");

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes);
    mobility.Install(wifiApNode);

    InternetStackHelper stack;
    stack.Install(wifiStaNodes);
    stack.Install(wifiApNode);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign(apDevice);

    // Create a UDP server (sink) on the AP node
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    Ptr<Socket> recvSocket = Socket::CreateSocket(wifiApNode.Get(0), tid);
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), port);
    recvSocket->Bind(local);
    recvSocket->SetRecvCallback(MakeCallback(&RxPacketLogger));

    // Create UDP client apps on each STA node
    for (uint32_t i = 0; i < nClients; ++i)
    {
        UdpClientHelper clientHelper(apInterface.GetAddress(0), port);
        clientHelper.SetAttribute("MaxPackets", UintegerValue(32));
        clientHelper.SetAttribute("Interval", TimeValue(Seconds(interval)));
        clientHelper.SetAttribute("PacketSize", UintegerValue(packetSize));
        ApplicationContainer clientApp = clientHelper.Install(wifiStaNodes.Get(i));
        clientApp.Start(Seconds(1.0 + i * 0.1));
        clientApp.Stop(Seconds(simulationTime));
    }

    Simulator::Stop(Seconds(simulationTime + 1));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}