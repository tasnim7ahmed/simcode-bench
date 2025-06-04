#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-address-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimpleLteExample");

int main(int argc, char *argv[])
{
    // Enable log components
    LogComponentEnable("SimpleLteExample", LOG_LEVEL_INFO);

    // Create nodes: UE (User Equipment), eNodeB (LTE base station), and EPC
    NodeContainer ueNodes, enbNodes, epcNodes;
    ueNodes.Create(1);
    enbNodes.Create(1);
    epcNodes.Create(1);

    // Set up LTE helpers
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<EpcHelper> epcHelper = CreateObject<EpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Set up the eNodeB (base station) and UE
    NetDeviceContainer ueDevices, enbDevices;
    enbDevices = lteHelper->InstallEnbDevice(enbNodes);
    ueDevices = lteHelper->InstallUeDevice(ueNodes);

    // Install the internet stack (IP, TCP, UDP)
    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(enbNodes);
    internet.Install(epcNodes);

    // Assign IP addresses to the devices
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueDevices);
    Ipv4InterfaceContainer enbIpIface = ipv4.Assign(enbDevices);

    // Attach UE to the eNodeB
    lteHelper->Attach(ueDevices.Get(0), enbDevices.Get(0));

    // Set up UDP server on the UE node (User Equipment)
    uint16_t port = 9;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(ueNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up UDP client on the eNodeB node
    UdpClientHelper udpClient(ueIpIface.GetAddress(0), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = udpClient.Install(enbNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
