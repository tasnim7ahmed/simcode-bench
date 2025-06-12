#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/lte-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteTcpSimulation");

int main(int argc, char *argv[]) {
    uint16_t numUes = 2;
    uint16_t numEnb = 1;
    Time simDuration = Seconds(10.0);

    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpNewReno::GetTypeId()));
    Config::SetDefault("ns3::LteHelper::UseIdealRrc", BooleanValue(true));
    Config::SetDefault("ns3::LteSidelinkHelper::UseSlIdealRrc", BooleanValue(true));

    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);
    lteHelper->SetAttribute("PathlossModel", StringValue("ns3::FriisSpectrumPropagationLossModel"));

    NodeContainer ues;
    ues.Create(numUes);

    NodeContainer enbs;
    enbs.Create(numEnb);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(ues);
    mobility.Install(enbs);

    NetDeviceContainer ueDevs;
    NetDeviceContainer enbDevs;

    enbDevs = lteHelper->InstallEnbDevice(enbs);
    ueDevs = lteHelper->InstallUeDevice(ues);

    lteHelper->Attach(ueDevs, enbDevs.Get(0));

    epcHelper->AssignIpv4Addresses(NetDeviceContainer(ueDevs));

    Ipv4InterfaceContainer ueIpIfaces;
    ueIpIfaces = epcHelper->AssignIpv4Addresses(ueDevs);

    Ipv4Address sinkAddress = ueIpIfaces.GetAddress(1, 0);
    uint16_t sinkPort = 8080;

    Address sinkLocalAddress(InetSocketAddress(sinkAddress, sinkPort));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = sinkHelper.Install(ues.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(simDuration);

    OnOffHelper client("ns3::TcpSocketFactory", InetSocketAddress(sinkAddress, sinkPort));
    client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    client.SetAttribute("DataRate", DataRateValue(DataRate("100Kbps")));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(ues.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(simDuration);

    lteHelper->EnableTraces();
    epcHelper->EnablePcapAll("lte_tcp_simulation");

    AnimationInterface anim("lte_tcp_simulation.xml");
    anim.SetMobilityPollInterval(Seconds(0.5));

    for (uint32_t i = 0; i < ues.GetN(); ++i) {
        anim.UpdateNodeDescription(ues.Get(i), "UE");
        anim.UpdateNodeColor(ues.Get(i), 255, 0, 0); // Red
    }
    for (uint32_t i = 0; i < enbs.GetN(); ++i) {
        anim.UpdateNodeDescription(enbs.Get(i), "eNB");
        anim.UpdateNodeColor(enbs.Get(i), 0, 0, 255); // Blue
    }

    Simulator::Stop(simDuration);
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}