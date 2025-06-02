#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/epc-helper.h"
#include "ns3/applications-module.h"
#include "ns3/nr-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Nr5GExample");

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("Nr5GExample", LOG_LEVEL_INFO);

    // Create nodes: gNB (Base Station) and UE (User Equipment)
    NodeContainer gNbNode, ueNode;
    gNbNode.Create(1);
    ueNode.Create(1);

    // Create EPC (Evolved Packet Core) helper and install it
    Ptr<EpcHelper> epcHelper = CreateObject<EpcHelper>();
    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();

    // Install NR stack
    nrHelper->Install(gNbNode);
    nrHelper->Install(ueNode);

    // Setup mobility for gNB and UE
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(gNbNode);
    mobility.Install(ueNode);

    // Install Internet Stack (IP) on UE
    InternetStackHelper internet;
    internet.Install(ueNode);

    // Setup UE and gNB network interface
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer ueIpInterface = ipv4.Assign(ueNode);

    // Configure and install applications
    uint16_t port = 8080;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(gNbNode);
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Setup UDP client on UE to send data to gNB
    UdpClientHelper udpClient(ueIpInterface.GetAddress(0), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(100));
    udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(500)));
    udpClient.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApp = udpClient.Install(ueNode.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
