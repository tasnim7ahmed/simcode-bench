#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimpleLTEExample");

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create nodes: 1 eNodeB (LTE base station) + 2 UE (User Equipments)
    NodeContainer ueNodes, enbNode;
    ueNodes.Create(2);
    enbNode.Create(1);

    // Install LTE stack
    LteHelper lte;
    NetDeviceContainer ueDevices, enbDevices;

    // Install LTE devices on UE and eNodeB
    enbDevices = lte.InstallEnbDevice(enbNode);
    ueDevices = lte.InstallUeDevice(ueNodes);

    // Install the Internet stack
    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(enbNode);

    // Assign IP addresses
    Ipv4InterfaceContainer ueIpInterface;
    ueIpInterface = lte.AssignUeIpv4Address(ueDevices);

    // Attach the UEs to the eNodeB
    lte.Attach(ueDevices.Get(0), enbDevices.Get(0));
    lte.Attach(ueDevices.Get(1), enbDevices.Get(0));

    // Configure the TCP server on UE 0
    uint16_t port = 8080;
    Address serverAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper tcpSink("ns3::TcpSocketFactory", serverAddress);
    ApplicationContainer serverApp = tcpSink.Install(ueNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Configure the TCP client on UE 1
    BulkSendHelper tcpClient("ns3::TcpSocketFactory", InetSocketAddress(ueIpInterface.GetAddress(0), port));
    tcpClient.SetAttribute("MaxBytes", UintegerValue(1000000));
    ApplicationContainer clientApp = tcpClient.Install(ueNodes.Get(1));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
