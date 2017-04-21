SELECT jsonknife_date_bound('1980', 'min');
SELECT jsonknife_date_bound('1980-02', 'min');
SELECT jsonknife_date_bound('1980-02-05','min');
SELECT jsonknife_date_bound('1980-02-05T08', 'min');
SELECT jsonknife_date_bound('1980-02-05T08:30', 'min');

SELECT jsonknife_date_bound(_at, 'min');
SELECT jsonknife_date_bound(_at, 'max');



SELECT jsonknife_date_bound('1980', 'max');
SELECT jsonknife_date_bound('1980-02', 'max');
SELECT jsonknife_date_bound('1980-02-05','max');
SELECT jsonknife_date_bound('1980-02-05T08', 'max');
SELECT jsonknife_date_bound('1980-02-05T08:30', 'max');


select jsonknife_date_bound('2000-01-01', 'min');
select jsonknife_date_bound('2000-01-01', 'max');
