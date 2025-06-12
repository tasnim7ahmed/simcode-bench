#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiExample");

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("WifiExample", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Setup Wi-Fi helper
    WifiHelper wifiHelper;
    wifiHelper.SetRemoteStationManager("ns3::AarfWifiManager");

    // Setup Wi-Fi PHY and MAC layers
    YansWifiPhyHelper wifiPhyHelper = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannelHelper = YansWifiChannelHelper::Default();
    wifiPhyHelper.SetChannel(wifiChannelHelper.Create());

    WifiMacHelper wifiMacHelper;
    wifiMacHelper.SetType("ns3::StaWifiMac",
                          "ActiveProbing", BooleanValue(false));

    // Install Wi-Fi devices on nodes
    NetDeviceContainer devices = wifiHelper.Install(wifiPhyHelper, wifiMacHelper, nodes);

    // Setup mobility model for nodes
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses to the devices
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Set up UDP server on Node B
    uint16_t port = 9;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(nodes.Get(1)); // Node B
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up UDP client on Node A
    UdpClientHelper udpClient(interfaces.GetAddress(1), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(100));
    udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
    udpClient.SetAttribute("PacketSize", UintegerValue(128));
    ApplicationContainer clientApp = udpClient.Install(nodes.Get(0)); // Node A
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Enable tracing
    wifiPhyHelper.EnablePcap("wifi_network", devices);

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
