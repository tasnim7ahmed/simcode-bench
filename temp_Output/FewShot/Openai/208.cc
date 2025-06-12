#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/yans-wifi-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiHandoverSimulation");

void
PhyRxDrop(Ptr<const Packet> p)
{
    static uint32_t drops = 0;
    ++drops;
    NS_LOG_UNCOND("Packet loss event: Total dropped = " << drops);
}

void
AssociationCallback(std::string context, Mac48Address oldAp, Mac48Address newAp)
{
    NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s: Node " 
        << context.substr(context.find_last_of('/')+1)
        << " handover from AP " << oldAp << " to " << newAp);
}

int main(int argc, char *argv[])
{
    uint32_t numSta = 6;
    double simTime = 30.0;
    double radius = 40.0;

    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer wifiApNodes;
    wifiApNodes.Create(2);
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(numSta);

    // Install Wi-Fi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;

    Ssid ssids[2] = {Ssid("AP1"), Ssid("AP2")};

    NetDeviceContainer apDevices, staDevices;

    for (uint32_t i = 0; i < 2; ++i)
    {
        // AP
        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssids[i]));
        apDevices.Add(wifi.Install(phy, mac, wifiApNodes.Get(i)));
    }
    // STAs
    for (uint32_t i = 0; i < numSta; ++i)
    {
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssids[0]));
        staDevices.Add(wifi.Install(phy, mac, wifiStaNodes.Get(i)));
    }

    // Mobility for APs
    MobilityHelper mobilityAp;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(20.0, 50.0, 0.0)); // AP1
    positionAlloc->Add(Vector(80.0, 50.0, 0.0)); // AP2
    mobilityAp.SetPositionAllocator(positionAlloc);
    mobilityAp.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityAp.Install(wifiApNodes);

    // Mobility for STAs
    MobilityHelper mobilitySta;
    mobilitySta.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                                      "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                      "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    mobilitySta.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                                  "Mode", StringValue ("Time"),
                                  "Time", TimeValue (Seconds (1.0)),
                                  "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=2.0]"),
                                  "Bounds", RectangleValue (Rectangle (0, 100, 0, 100)));
    mobilitySta.Install(wifiStaNodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiApNodes);
    stack.Install(wifiStaNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apIfs, staIfs;
    NetDeviceContainer allDevices;
    allDevices.Add(apDevices);
    allDevices.Add(staDevices);
    Ipv4InterfaceContainer allIfs = address.Assign(allDevices);

    // Install UDP Echo Server on AP1
    uint16_t echoPort = 9;
    UdpEchoServerHelper echoServer(echoPort);
    ApplicationContainer serverApps = echoServer.Install(wifiApNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simTime));

    // Install UDP Echo Client on all STAs
    for (uint32_t i = 0; i < numSta; ++i)
    {
        UdpEchoClientHelper echoClient(allIfs.GetAddress(0), echoPort);
        echoClient.SetAttribute("MaxPackets", UintegerValue(1000));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(64));
        ApplicationContainer clientApps = echoClient.Install(wifiStaNodes.Get(i));
        clientApps.Start(Seconds(2.0+i*0.1));
        clientApps.Stop(Seconds(simTime));
    }

    // Enable packet drops trace (measure packet loss)
    phy.EnablePcapAll("wifi-handover");

    for (uint32_t i = 0; i < staDevices.GetN(); ++i)
    {
        std::ostringstream oss;
        oss << "/NodeList/" << wifiStaNodes.Get(i)->GetId() << "/DeviceList/0/Phy/PhyRxDrop";
        Config::Connect(oss.str(), MakeCallback(&PhyRxDrop));
    }

    // Track handover/association events
    for (uint32_t i = 0; i < numSta; ++i)
    {
        std::ostringstream oss;
        oss << "/NodeList/" << wifiStaNodes.Get(i)->GetId() <<
            "/DeviceList/0/Mac/MacAssoc";
        Config::Connect(oss.str(), MakeBoundCallback(&AssociationCallback, 
            "sta"+std::to_string(i)));
    }

    // Enable NetAnim visualization
    AnimationInterface anim("wifi-handover.xml");
    for (uint32_t i = 0; i < wifiApNodes.GetN(); ++i)
    {
        anim.UpdateNodeDescription(wifiApNodes.Get(i), "AP"+std::to_string(i+1));
        anim.UpdateNodeColor(wifiApNodes.Get(i), 255,0,0);
    }
    for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i)
    {
        anim.UpdateNodeDescription(wifiStaNodes.Get(i), "STA"+std::to_string(i+1));
        anim.UpdateNodeColor(wifiStaNodes.Get(i), 0,0,255);
    }

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}