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

    // Create nodes: UE (User Equipment) and eNB (evolved NodeB)
    NodeContainer ueNodes, enbNodes;
    ueNodes.Create(1);
    enbNodes.Create(1);

    // Set up LTE helper
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();

    // Set up EPC helper (Evolved Packet Core)
    Ptr<EpcHelper> epcHelper = CreateObject<EpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Set up the LTE network
    NetDeviceContainer ueDevices, enbDevices;
    ueDevices = lteHelper->InstallUeDevice(ueNodes);
    enbDevices = lteHelper->InstallEnbDevice(enbNodes);

    // Install the internet stack (IP, TCP, UDP)
    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(enbNodes);

    // Assign IP addresses to the devices
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueDevices);
    Ipv4InterfaceContainer enbIpIface = ipv4.Assign(enbDevices);

    // Attach the UE to the eNB
    lteHelper->Attach(ueDevices.Get(0), enbDevices.Get(0));

    // Set up UDP server on the eNB node (evolved NodeB)
    uint16_t port = 9;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(enbNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up UDP client on the UE node (User Equipment)
    UdpClientHelper udpClient(enbIpIface.GetAddress(0), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = udpClient.Install(ueNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
