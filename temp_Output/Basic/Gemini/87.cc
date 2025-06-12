#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/config-store-module.h"
#include "ns3/netanim-module.h"
#include "ns3/sqlite3-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiDistanceExperiment");

// DataCollector class to manage simulation results
class DataCollector {
public:
    DataCollector(std::string format, std::string runId) : m_format(format), m_runId(runId) {}

    void CollectData(double distance, double txPackets, double rxPackets, double delay) {
        m_distance = distance;
        m_txPackets = txPackets;
        m_rxPackets = rxPackets;
        m_delay = delay;
    }

    void WriteData() {
        if (m_format == "omnet") {
            WriteOmnetData();
        } else if (m_format == "sqlite") {
            WriteSqliteData();
        } else {
            std::cerr << "Error: Invalid format specified." << std::endl;
        }
    }

private:
    void WriteOmnetData() {
        std::ofstream outputFile(m_runId + ".sca");
        if (outputFile.is_open()) {
            outputFile << "attr distance " << m_distance << std::endl;
            outputFile << "scalar txPackets " << m_txPackets << std::endl;
            outputFile << "scalar rxPackets " << m_rxPackets << std::endl;
            outputFile << "scalar delay " << m_delay << std::endl;
            outputFile.close();
        } else {
            std::cerr << "Error: Unable to open output file." << std::endl;
        }
    }

    void WriteSqliteData() {
        Sqlite3Helper dbHelper;
        dbHelper.Open(m_runId + ".db");

        std::stringstream ss;
        ss << "CREATE TABLE IF NOT EXISTS wifi_results (distance REAL, tx_packets INTEGER, rx_packets INTEGER, delay REAL);";
        dbHelper.Exec(ss.str());

        ss.str("");
        ss << "INSERT INTO wifi_results VALUES (" << m_distance << ", " << m_txPackets << ", " << m_rxPackets << ", " << m_delay << ");";
        dbHelper.Exec(ss.str());

        dbHelper.Close();
    }

    std::string m_format;
    std::string m_runId;
    double m_distance;
    double m_txPackets;
    double m_rxPackets;
    double m_delay;
};

double txPacketsGlobal = 0;
double rxPacketsGlobal = 0;
double delaySumGlobal = 0;
uint32_t rxPacketCount = 0;

void PhyTxTrace(std::string context, Ptr<const Packet> packet) {
    txPacketsGlobal++;
}

void PhyRxTrace(std::string context, Ptr<const Packet> packet) {
    rxPacketsGlobal++;
}

void DelayTrace(std::string context, Time delay) {
    delaySumGlobal += delay.GetSeconds();
    rxPacketCount++;
}


int main (int argc, char *argv[])
{
    bool verbose = false;
    double distance = 10.0;
    std::string format = "omnet";
    std::string runId = "run1";

    CommandLine cmd;
    cmd.AddValue ("verbose", "Tell echo applications to log if true", verbose);
    cmd.AddValue ("distance", "Distance between nodes (meters)", distance);
    cmd.AddValue ("format", "Output format (omnet or sqlite)", format);
    cmd.AddValue ("runId", "Run identifier", runId);
    cmd.Parse (argc, argv);

    Config::SetDefault ("ns3::WifiMac::Ssid", SsidValue (Ssid ("ns-3-ssid")));

    NodeContainer nodes;
    nodes.Create (2);

    WifiHelper wifi;
    wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create ();
    wifiPhy.SetChannel (wifiChannel.Create ());

    WifiMacHelper wifiMac;
    wifiMac.SetType ("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                     "MinX", DoubleValue (0.0),
                                     "MinY", DoubleValue (0.0),
                                     "DeltaX", DoubleValue (distance),
                                     "DeltaY", DoubleValue (0.0),
                                     "GridWidth", UintegerValue (3),
                                     "LayoutType", StringValue ("RowFirst"));
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (nodes);

    InternetStackHelper stack;
    stack.Install (nodes);

    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign (devices);

    UdpEchoServerHelper echoServer (9);
    ApplicationContainer serverApps = echoServer.Install (nodes.Get (1));
    serverApps.Start (Seconds (1.0));
    serverApps.Stop (Seconds (10.0));

    UdpEchoClientHelper echoClient (interfaces.GetAddress (1), 9);
    echoClient.SetAttribute ("MaxPackets", UintegerValue (100));
    echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
    echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
    ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
    clientApps.Start (Seconds (2.0));
    clientApps.Stop (Seconds (10.0));

    // Tracing
    Config::ConnectWithoutContext ("/NodeList/0/DeviceList/*/$ns3::WifiNetDevice/Mac/MacRx", MakeCallback (&PhyRxTrace));
    Config::ConnectWithoutContext ("/NodeList/0/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyTxBegin", MakeCallback (&PhyTxTrace));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

    // Packet Delay Tracing
    for (uint32_t i = 0; i < devices.GetN (); ++i)
    {
        Ptr<WifiNetDevice> wifiNetDevice = DynamicCast<WifiNetDevice> (devices.Get (i));
        Ptr<WifiMac> wifiMac = wifiNetDevice->GetMac ();
        wifiMac->TraceConnectWithoutContext ("MacRxDelay", "Receiver Delay", MakeCallback (&DelayTrace));
    }

    Simulator::Stop (Seconds (11.0));
    Simulator::Run ();

    monitor->CheckForLostPackets ();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

    double delay = (rxPacketCount > 0) ? delaySumGlobal / rxPacketCount : 0;

    DataCollector dataCollector(format, runId);
    dataCollector.CollectData(distance, txPacketsGlobal, rxPacketsGlobal, delay);
    dataCollector.WriteData();

    Simulator::Destroy ();
    return 0;
}