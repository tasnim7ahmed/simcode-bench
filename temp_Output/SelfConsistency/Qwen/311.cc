#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteTcpServerClientSimulation");

int main(int argc, char *argv[])
{
    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create eNodeB and UE nodes
    NodeContainer enbNodes;
    enbNodes.Create(1);

    NodeContainer ueNodes;
    ueNodes.Create(2);

    // Install LTE devices
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    lteHelper->SetSchedulerType("ns3::PfFfMacScheduler");

    NetDeviceContainer enbDevs;
    enbDevs = lteHelper->InstallEnbDevice(enbNodes);

    NetDeviceContainer ueDevs;
    ueDevs = lteHelper->InstallUeDevice(ueNodes);

    // Attach UEs to eNodeB
    lteHelper->Attach(ueDevs, enbDevs.Get(0));

    // Install internet stack
    InternetStackHelper internet;
    internet.Install(ueNodes);

    // Assign IP addresses
    Ipv4InterfaceContainer ueInterfaces;
    ueInterfaces = lteHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Set default gateway for UEs
    for (uint32_t u = 0; u < ueNodes.GetN(); ++u)
    {
        Ptr<Node> ueNode = ueNodes.Get(u);
        Ptr<Ipv4StaticRouting> routing = ipv4StaticRoutingHelper.GetStaticRouting(ueNode->GetObject<Ipv4>());
        routing->SetDefaultRoute(enbNodes.Get(0)->GetObject<Ipv4>()->GetAddress(1, 0), 1);
    }

    // Setup TCP server on UE 0
    uint16_t port = 8080;
    Address localAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", localAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(ueNodes.Get(0));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    // Setup TCP client on UE 1
    Address remoteAddress(InetSocketAddress(ueInterfaces.GetAddress(0), port));
    BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory", remoteAddress);
    bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(1000000));
    ApplicationContainer clientApp = bulkSendHelper.Install(ueNodes.Get(1));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Enable LTE logging
    lteHelper->EnableTraces();

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}