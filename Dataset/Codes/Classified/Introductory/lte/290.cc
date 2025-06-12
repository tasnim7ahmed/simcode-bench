#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteBasicExample");

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("LteBasicExample", LOG_LEVEL_INFO);

    // Create LTE components: eNodeB and UE
    NodeContainer ueNode, enbNode;
    ueNode.Create(1);
    enbNode.Create(1);

    // Configure LTE module
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();

    // Install LTE devices on UE and eNodeB
    NetDeviceContainer enbDevice = lteHelper->InstallEnbDevice(enbNode);
    NetDeviceContainer ueDevice = lteHelper->InstallUeDevice(ueNode);

    // Configure mobility for eNodeB (fixed) and UE (mobile)
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNode);

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-100, 100, -100, 100)));
    mobility.Install(ueNode);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(ueNode);

    // Assign IP addresses
    Ipv4InterfaceContainer ueIpIface = lteHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevice));

    // Attach UE to eNodeB
    lteHelper->Attach(ueDevice, enbDevice.Get(0));

    // Setup UDP server on UE
    uint16_t port = 5000;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(ueNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Setup UDP client on eNodeB
    UdpClientHelper udpClient(ueIpIface.GetAddress(0), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(100));
    udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(200)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = udpClient.Install(enbNode.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Enable PCAP tracing
    lteHelper->EnablePhyTraces();
    lteHelper->EnableMacTraces();
    lteHelper->EnableRlcTraces();

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
