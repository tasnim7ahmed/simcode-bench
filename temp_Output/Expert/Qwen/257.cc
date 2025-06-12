#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    uint16_t numEnb = 1;
    uint16_t numUe = 2;
    Time simDuration = Seconds(10.0);

    Config::SetDefault("ns3::LteHelper::UseRlcSm", BooleanValue(true));
    Config::SetDefault("ns3::LteSpectrumPhy::CtrlErrorModelEnabled", BooleanValue(false));
    Config::SetDefault("ns3::LteSpectrumPhy::DataErrorModelEnabled", BooleanValue(false));

    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);
    lteHelper->SetSchedulerType("ns3::PfFfMacScheduler");

    NodeContainer enbNodes;
    enbNodes.Create(numEnb);

    NodeContainer ueNodes;
    ueNodes.Create(numUe);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNodes);
    mobility.Install(ueNodes);

    NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

    epcHelper->AddEnb(enbNodes.Get(0), enbDevs.Get(0), 0);
    epcHelper->AddUe(ueNodes.Get(0), ueDevs.Get(0));
    epcHelper->AddUe(ueNodes.Get(1), ueDevs.Get(1));

    Ipv4InterfaceContainer ueIpIfaces = epcHelper->AssignUeIpv4Address(ueDevs);

    lteHelper->Attach(ueDevs, enbDevs.Get(0));

    uint16_t dlPort = 9;
    Address sinkLocalAddr(InetSocketAddress(Ipv4Address::GetAny(), dlPort));
    PacketSinkHelper dlPacketSinkHelper("ns3::UdpSocketFactory", sinkLocalAddr);
    ApplicationContainer dlSinkApps = dlPacketSinkHelper.Install(ueNodes.Get(1));
    dlSinkApps.Start(Seconds(0.0));
    dlSinkApps.Stop(simDuration);

    InetSocketAddress dlSinkSocketAddr(ueIpIfaces.GetAddress(1), dlPort);
    dlSinkSocketAddr.SetTos(0xb8);

    OnOffHelper dlClient("ns3::UdpSocketFactory", dlSinkSocketAddr);
    dlClient.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    dlClient.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    dlClient.SetAttribute("DataRate", DataRateValue(DataRate("1000000bps")));
    dlClient.SetAttribute("PacketSize", UintegerValue(512));
    dlClient.SetAttribute("MaxBytes", UintegerValue(2 * 512));

    ApplicationContainer dlClientApps = dlClient.Install(ueNodes.Get(0));
    dlClientApps.Start(Seconds(1.0));
    dlClientApps.Stop(simDuration);

    Simulator::Stop(simDuration);
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}