#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimpleLteExample");

int main(int argc, char *argv[])
{
    // Create LTE nodes
    NodeContainer enbNode, ueNode;
    enbNode.Create(1);
    ueNode.Create(1);

    // Set up LTE devices
    LteHelper lteHelper;
    NetDeviceContainer enbLteDevice = lteHelper.InstallEnbDevice(enbNode);
    NetDeviceContainer ueLteDevice = lteHelper.InstallUeDevice(ueNode);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(ueNode);
    internet.Install(enbNode);

    // Assign IP addresses
    Ipv4InterfaceContainer ueIpIfaces;
    ueIpIfaces = lteHelper.AssignUeIpv4Address(ueLteDevice);

    // Attach UE to eNB
    lteHelper.Attach(ueLteDevice, enbLteDevice.Get(0));

    // Set up the UDP server on the UE
    uint16_t port = 12345;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(ueNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up the UDP client on the eNB (simulating data transmission)
    UdpClientHelper udpClient(ueIpIfaces.GetAddress(0), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(100));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = udpClient.Install(enbNode.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(9.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
