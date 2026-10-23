import csv,tempfile,unittest,json
from pathlib import Path
from tools.summarize_quality import resources,profiles
class QualitySummaryTests(unittest.TestCase):
 def metrics(self,rows):
  tmp=tempfile.TemporaryDirectory();self.addCleanup(tmp.cleanup);p=Path(tmp.name)/'metrics.csv'
  columns=['elapsed_seconds','cpu_one_core_percent','working_set_bytes','private_bytes','decode_count','render_submissions','process_handles','gdi_objects','render_paused']
  with p.open('w',encoding='utf-8-sig',newline='') as f:
   w=csv.writer(f);w.writerow(columns)
   for time,cpu,frames,paused in rows:w.writerow([time,cpu,1000000,0,5,frames,0,0,paused])
  return p
 def test_paused_and_long_unobserved_gaps_do_not_dilute_cpu(self):
  p=self.metrics([(60,0,1,0),(61,10,2,0),(62,0,2,1),(63,0,2,1),(163,0,2,0),(164,20,3,0)])
  result=resources(p,wine=True)
  self.assertEqual(result['cpu_one_core_average_percent'],15)
  self.assertEqual(result['active_cpu_observed_seconds'],2)
  self.assertEqual(len(result['sample_gaps_over_5_seconds']),1)
  self.assertIsNone(result['performance_targets']['cpu_one_core_1_percent'])
  self.assertFalse(result['counters']['private_bytes']['available'])
 def test_rejects_reversed_time_and_process_counter_reset(self):
  for rows in [[(61,1,2,0),(60,1,3,0)],[(60,1,4,0),(61,1,2,0)]]:
   with self.assertRaises(ValueError):resources(self.metrics(rows))
 def test_native_goal_uses_unrounded_single_core_cpu(self):
  r=resources(self.metrics([(60,0,0,0),(61,1.01,1,0)]),wine=False)
  self.assertFalse(r['performance_targets']['cpu_one_core_1_percent'])
  self.assertIsNone(r['performance_targets']['private_30MB'])
 def test_incomplete_profile_matrix_cannot_be_reported_complete(self):
  tmp=tempfile.TemporaryDirectory();self.addCleanup(tmp.cleanup);p=Path(tmp.name)/'cases.jsonl';p.write_text(json.dumps({'root':1,'solo':False,'seed':41})+'\n')
  with self.assertRaises(ValueError):profiles(p)
