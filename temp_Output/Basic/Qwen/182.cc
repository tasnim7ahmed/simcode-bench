#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/lte-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteTcpSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create nodes: 2 UEs and 1 eNodeB
    NodeContainer ueNodes;
    ueNodes.Create(2);
    NodeContainer enbNode;
    enbNode.Create(1);

    // Create LTE helper
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Install LTE devices
    NetDeviceContainer enbDevs;
    enbDevs = lteHelper->InstallEnbDevice(enbNode);
    NetDeviceContainer ueDevs;
    ueDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install the IP stack on the UEs
    InternetStackHelper internet;
    internet.Install(ueNodes);

    Ipv4InterfaceContainer ueIpIfaces;
    ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Attach UEs to the first eNodeB
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i) {
        lteHelper->Attach(ueDevs.Get(i), enbDevs.Get(0));
    }

    // Setup TCP applications
    uint16_t port = 5000;
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApps;

    for (uint32_t i = 0; i < ueNodes.GetN(); ++i) {
        sinkApps.Add(packetSinkHelper.Install(ueNodes.Get(i)));
    }
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    OnOffHelper clientHelper("ns3::TcpSocketFactory", InetSocketAddress());
    clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i) {
        AddressValue remoteAddress(InetSocketAddress(ueIpIfaces.GetAddress((i + 1) % 2), port));
        clientHelper.SetAttribute("Remote", remoteAddress);
        clientApps.Add(clientHelper.Install(ueNodes.Get(i)));
    }
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    // Set mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(ueNodes);
    mobility.Install(enbNode);

    // Enable NetAnim visualization
    AnimationInterface anim("lte-tcp-animation.xml");
    anim.SetMobilityPollInterval(Seconds(0.5));

    // Assign node colors in animation
    anim.UpdateNodeDescription(enbNode.Get(0), "eNodeB");
    anim.UpdateNodeColor(enbNode.Get(0), 255, 0, 0); // Red

    for (uint32_t i = 0; i < ueNodes.GetN(); ++i) {
        std::ostringstream oss;
        oss << "UE" << i;
        anim.UpdateNodeDescription(ueNodes.Get(i), oss.str());
        anim.UpdateNodeColor(ueNodes.Get(i), 0, 0, 255); // Blue
    }

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}