#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mesh-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MeshExample");

uint32_t g_txPackets = 0;
uint32_t g_rxPackets = 0;

void TxTrace(Ptr<const Packet> p)
{
    g_txPackets++;
}

void RxTrace(Ptr<const Packet> p, const Address &address)
{
    g_rxPackets++;
}

int main(int argc, char *argv[])
{
    // LogComponentEnable("MeshExample", LOG_LEVEL_INFO);

    // Simulation parameters
    uint32_t numNodes = 4;
    double distance = 20.0; // meters between nodes in grid
    double simulationTime = 15.0;

    // Create nodes
    NodeContainer meshNodes;
    meshNodes.Create(numNodes);

    // Wifi and mesh helpers
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    MeshHelper mesh = MeshHelper::Default();
    mesh.SetStackInstaller("ns3::Dot11sStack");
    mesh.SetMacType("RandomStart", TimeValue(Seconds(0.1)));
    mesh.SetNumberOfInterfaces(1);

    NetDeviceContainer meshDevices = mesh.Install(wifiPhy, meshNodes);

    // Mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(meshNodes);

    // 2x2 grid positions
    Ptr<ConstantPositionMobilityModel> mob0 = meshNodes.Get(0)->GetObject<ConstantPositionMobilityModel>();
    Ptr<ConstantPositionMobilityModel> mob1 = meshNodes.Get(1)->GetObject<ConstantPositionMobilityModel>();
    Ptr<ConstantPositionMobilityModel> mob2 = meshNodes.Get(2)->GetObject<ConstantPositionMobilityModel>();
    Ptr<ConstantPositionMobilityModel> mob3 = meshNodes.Get(3)->GetObject<ConstantPositionMobilityModel>();
    mob0->SetPosition(Vector(0.0, 0.0, 0.0));
    mob1->SetPosition(Vector(distance, 0.0, 0.0));
    mob2->SetPosition(Vector(0.0, distance, 0.0));
    mob3->SetPosition(Vector(distance, distance, 0.0));

    // Install internet stack and assign IP addresses
    InternetStackHelper internetStack;
    internetStack.Install(meshNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(meshDevices);

    // UDP applications
    uint16_t port = 9000;

    // Install a UDP server on node 3
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(meshNodes.Get(3));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(simulationTime));

    // Install a UDP client on node 0, send to node 3
    UdpClientHelper udpClient(interfaces.GetAddress(3), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(320)); // e.g., 20 packets per second for 14 seconds
    udpClient.SetAttribute("Interval", TimeValue(Seconds(0.05)));
    udpClient.SetAttribute("PacketSize", UintegerValue(128));
    ApplicationContainer clientApp = udpClient.Install(meshNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(simulationTime - 1.0));

    // Trace packet transmission/reception
    meshDevices.Get(0)->TraceConnectWithoutContext("PhyTxBegin", MakeCallback(&TxTrace));
    meshDevices.Get(3)->TraceConnectWithoutContext("PhyRxEnd", MakeCallback(&RxTrace));

    // Enable PCAP tracing
    wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
    wifiPhy.EnablePcap("mesh-node0", meshDevices.Get(0));
    wifiPhy.EnablePcap("mesh-node1", meshDevices.Get(1));
    wifiPhy.EnablePcap("mesh-node2", meshDevices.Get(2));
    wifiPhy.EnablePcap("mesh-node3", meshDevices.Get(3));

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // Retrieve statistics from the UDP server
    Ptr<UdpServer> server = DynamicCast<UdpServer>(serverApp.Get(0));
    uint32_t totalRx = server->GetReceived();

    std::cout << "Packets transmitted by node 0: " << g_txPackets << std::endl;
    std::cout << "Packets received by node 3 (UDP app): " << totalRx << std::endl;
    std::cout << "Packets received at PHY by node 3: " << g_rxPackets << std::endl;

    Simulator::Destroy();
    return 0;
}