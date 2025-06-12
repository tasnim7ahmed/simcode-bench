#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for debugging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create LTE helper components
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Set the scheduler type
    lteHelper->SetSchedulerType("ns3::PfFfMacScheduler");

    // Create nodes: 2 UEs and 1 eNodeB
    NodeContainer enbNodes;
    NodeContainer ueNodes;
    enbNodes.Create(1);
    ueNodes.Create(2);

    // Install mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNodes);
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.Install(ueNodes);

    // Install LTE devices
    NetDeviceContainer enbDevs;
    NetDeviceContainer ueDevs;

    enbDevs = lteHelper->InstallEnbDevice(enbNodes);
    ueDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(ueNodes);

    // Assign IP addresses
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Attach UEs to eNodeB
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i) {
        lteHelper->Attach(ueDevs.Get(i), enbDevs.Get(0));
    }

    // Set up TCP applications
    uint16_t dlPort = 1234;
    uint16_t ulPort = 2000;
    uint16_t port = 3000;

    // Downlink: client on UE 0, server on UE 1
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), dlPort));
    PacketSinkHelper dlPacketSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer dlSinkApps = dlPacketSinkHelper.Install(ueNodes.Get(1));
    dlSinkApps.Start(Seconds(0.0));
    dlSinkApps.Stop(Seconds(10.0));

    InetSocketAddress dlRemoteAddress(ueIpIface.GetAddress(1), dlPort);
    dlRemoteAddress.SetTos(0xb8);
    BulkSendHelper dlSource("ns3::TcpSocketFactory", dlRemoteAddress);
    dlSource.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer dlApp = dlSource.Install(ueNodes.Get(0));
    dlApp.Start(Seconds(2.0));
    dlApp.Stop(Seconds(10.0));

    // Uplink: client on UE 1, server on UE 0
    Address ulSinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), ulPort));
    PacketSinkHelper ulPacketSinkHelper("ns3::TcpSocketFactory", ulSinkLocalAddress);
    ApplicationContainer ulSinkApps = ulPacketSinkHelper.Install(ueNodes.Get(0));
    ulSinkApps.Start(Seconds(0.0));
    ulSinkApps.Stop(Seconds(10.0));

    InetSocketAddress ulRemoteAddress(ueIpIface.GetAddress(0), ulPort);
    ulRemoteAddress.SetTos(0xb8);
    BulkSendHelper ulSource("ns3::TcpSocketFactory", ulRemoteAddress);
    ulSource.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer ulApp = ulSource.Install(ueNodes.Get(1));
    ulApp.Start(Seconds(3.0));
    ulApp.Stop(Seconds(10.0));

    // Animation
    AnimationInterface anim("lte-tcp-animation.xml");
    anim.SetMobilityPollInterval(Seconds(0.5));

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}