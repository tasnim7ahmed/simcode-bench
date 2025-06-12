#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/gnb-module.h"
#include "ns3/ue-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-address-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Simple5GExample");

int main(int argc, char *argv[])
{
    // Enable log components
    LogComponentEnable("Simple5GExample", LOG_LEVEL_INFO);

    // Create nodes: UE (User Equipment), gNB (5G Base Station), and 5G Core Network
    NodeContainer ueNodes, gnbNodes, coreNetworkNodes;
    ueNodes.Create(1);
    gnbNodes.Create(1);
    coreNetworkNodes.Create(1);

    // Set up 5G NR helpers
    Ptr<GnbHelper> gnbHelper = CreateObject<GnbHelper>();
    Ptr<UeHelper> ueHelper = CreateObject<UeHelper>();
    Ptr<CoreNetworkHelper> coreNetworkHelper = CreateObject<CoreNetworkHelper>();
    gnbHelper->SetCoreNetworkHelper(coreNetworkHelper);

    // Set up the gNB (base station) and UE
    NetDeviceContainer ueDevices, gnbDevices;
    gnbDevices = gnbHelper->InstallGnbDevice(gnbNodes);
    ueDevices = ueHelper->InstallUeDevice(ueNodes);

    // Install the internet stack (IP, TCP, UDP)
    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(gnbNodes);
    internet.Install(coreNetworkNodes);

    // Assign IP addresses to the devices
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueDevices);
    Ipv4InterfaceContainer gnbIpIface = ipv4.Assign(gnbDevices);

    // Attach UE to the gNB (5G base station)
    gnbHelper->Attach(ueDevices.Get(0), gnbDevices.Get(0));

    // Set up UDP server on the UE node (User Equipment)
    uint16_t port = 9;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(ueNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up UDP client on the gNB node
    UdpClientHelper udpClient(ueIpIface.GetAddress(0), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = udpClient.Install(gnbNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
