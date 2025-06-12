#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/lte-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    double simDuration = 10.0;

    // Enable logging
    LogComponentEnable("UdpServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("HttpClientApplication", LOG_LEVEL_INFO);

    // Create nodes: one eNodeB and one UE
    NodeContainer enbNodes;
    enbNodes.Create(1);

    NodeContainer ueNodes;
    ueNodes.Create(1);

    // Setup mobility for UE with Random Walk in 50x50 area
    MobilityHelper mobilityUe;
    mobilityUe.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)),
                                "Distance", DoubleValue(5.0));
    mobilityUe.Install(ueNodes);

    // Setup static position for eNodeB
    MobilityHelper mobilityEnb;
    mobilityEnb.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityEnb.Install(enbNodes);
    enbNodes.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(0.0, 0.0, 0.0));

    // Create LTE helper
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    lteHelper->SetSchedulerType("ns3::RrFfMacScheduler");

    // Install LTE devices
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Attach UEs to eNodeBs
    lteHelper->Attach(ueLteDevs, enbLteDevs.Get(0));

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(enbNodes);

    // Assign IP addresses
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = lteHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

    // Set default gateway for UE
    Ipv4Address enbIpAddress = ueIpIface.GetAddress(0, 1); // Get the first eNB interface address
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i) {
        Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(ueNodes.Get(i)->GetObject<Ipv4>());
        ueStaticRouting->SetDefaultRoute(enbIpAddress, 1);
    }

    // Install UDP server on eNodeB at port 80
    UdpServerHelper udpServer(80);
    ApplicationContainer serverApps = udpServer.Install(enbNodes.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simDuration));

    // Install HTTP client on UE: send 5 packets at 1s intervals starting at 2s
    HttpClientHelper httpClientHelper(enbIpAddress, 80);
    httpClientHelper.SetAttribute("MaxPackets", UintegerValue(5));
    httpClientHelper.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    httpClientHelper.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = httpClientHelper.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simDuration));

    // Set simulation time and run
    Simulator::Stop(Seconds(simDuration));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}